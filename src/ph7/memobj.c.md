# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 889/1021 lines (87.07%)

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
|        4 |   38 | `		return 0;` |
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
|      372 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      377 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      329 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      321 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      235 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      225 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       25 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        3 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|      ! 0 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   75 | `	return "unknown";` |
|      191 |   76 | `}` |
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
|    10688 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|    10693 |  111 | `  ph7_real r = pObj->rVal;` |
|    10693 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|    10691 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      176 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|    10517 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     5349 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  3553106 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  3553111 |  131 | `	sxi64 iVal = 0;` |
|  3553111 |  132 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  3553111 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|  1382513 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|  1275049 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|   107469 |  140 | `		c = pVal->zString[1];` |
|   107469 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|   103283 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|    55830 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      281 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     4051 |  147 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|        - |  148 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|        - |  149 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|       17 |  150 | `			if( pVal->nByte > 2 ){` |
|       17 |  151 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        8 |  152 | `			}` |
|        9 |  153 | `		}else{` |
|        - |  154 | `			/* Legacy octal digit stream (leading 0) */` |
|     3895 |  155 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  156 | `		}` |
|    53737 |  157 | `	}else{` |
|        - |  158 | `		/* Decimal digit stream */` |
|  2170603 |  159 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  160 | `	}` |
|  2278067 |  161 | `	return iVal;` |
|  1776558 |  162 | `}` |
|        - |  163 | `/*` |
|        - |  164 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  165 | ` * do at representing the value that pObj describes as a string` |
|        - |  166 | ` * representation.` |
|        - |  167 | ` */` |
|     3432 |  168 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  169 | `{` |
|     3437 |  170 | `	sxi64 iVal = 0;` |
|        - |  171 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|        - |  172 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|        - |  173 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|     3437 |  174 | `	SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0);` |
|     3437 |  175 | `	return iVal;` |
|        5 |  176 | `}` |
|        - |  177 | `/*` |
|        - |  178 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  179 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  180 | ` * successfully called. Any other return value indicates failure.` |
|        - |  181 | ` */` |
|      356 |  182 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  183 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  184 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  185 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  186 | `	sxu32 nLen,                /* Method name length */` |
|        - |  187 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  188 | `	)` |
|        5 |  189 | `{` |
|        - |  190 | `	ph7_class_method *pMethod;` |
|        - |  191 | `	/* Check if the method is available */` |
|      361 |  192 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      361 |  193 | `	if( pMethod == 0 ){` |
|        - |  194 | `		/* No such method */` |
|      175 |  195 | `		return SXERR_NOTFOUND;` |
|        - |  196 | `	}` |
|        - |  197 | `	/* Invoke the desired method */` |
|      189 |  198 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  199 | `	/* Method successfully called,pResult should hold the return value */` |
|      189 |  200 | `	return SXRET_OK;` |
|      183 |  201 | `}` |
|        - |  202 | `/*` |
|        - |  203 | ` * Return some kind of integer value which is the best we can` |
|        - |  204 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  205 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  206 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  207 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  208 | ` * a integer and return that.` |
|        - |  209 | ` * If pObj represents a NULL value, return 0.` |
|        - |  210 | ` */` |
|     1342 |  211 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  212 | `{` |
|        - |  213 | `	sxi32 iFlags;` |
|     1347 |  214 | `	iFlags = pObj->iFlags;` |
|     1347 |  215 | `	if (iFlags & MEMOBJ_REAL ){` |
|       41 |  216 | `		return MemObjRealToInt(&(*pObj));` |
|     1307 |  217 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      170 |  218 | `		return pObj->x.iVal;` |
|     1139 |  219 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     1121 |  220 | `		return MemObjStringToInt(&(*pObj));` |
|       19 |  221 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        7 |  222 | `		return 0;` |
|       13 |  223 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  224 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  225 | `		sxu32 n = pMap->nEntry;` |
|        7 |  226 | `		PH7_HashmapUnref(pMap);` |
|        - |  227 | `		/* Return total number of entries in the hashmap */` |
|        7 |  228 | `		return n;` |
|        7 |  229 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  230 | `		ph7_value sResult;` |
|        5 |  231 | `		sxi64 iVal = 1;` |
|        - |  232 | `		sxi32 rc;` |
|        - |  233 | `		/* Invoke the [__toInt()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  234 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  235 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  236 | `			"__toInt",sizeof("__toInt")-1,&sResult);` |
|        5 |  237 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_INT) ){` |
|        - |  238 | `			/* Extract method return value */` |
|        5 |  239 | `			iVal = sResult.x.iVal;` |
|        2 |  240 | `		}` |
|        5 |  241 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  242 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  243 | `		return iVal;` |
|        3 |  244 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  245 | `		return pObj->x.pOther != 0;` |
|        - |  246 | `	}` |
|        - |  247 | `	/* CANT HAPPEN */` |
|      ! 0 |  248 | `	return 0;` |
|      676 |  249 | `}` |
|        - |  250 | `/*` |
|        - |  251 | ` * Return some kind of real value which is the best we can` |
|        - |  252 | ` * do at representing the value that pObj describes as a real.` |
|        - |  253 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  254 | ` * integer then the integer  is promoted to real and that value` |
|        - |  255 | ` * is returned.` |
|        - |  256 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  257 | ` * into a real and return that.` |
|        - |  258 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  259 | ` */` |
|     9476 |  260 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  261 | `{` |
|        - |  262 | `	sxi32 iFlags;` |
|     9481 |  263 | `	iFlags = pObj->iFlags;` |
|     9481 |  264 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  265 | `		return pObj->rVal;` |
|     9481 |  266 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      771 |  267 | `		return (ph7_real)pObj->x.iVal;` |
|     8713 |  268 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  269 | `		SyString sString;` |
|        - |  270 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  271 | `		ph7_real rVal = 0;` |
|        - |  272 | `#else` |
|     8707 |  273 | `		ph7_real rVal = 0.0;` |
|        - |  274 | `#endif` |
|     8707 |  275 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     8707 |  276 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  277 | `			/* Convert as much as we can */` |
|        - |  278 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  279 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  280 | `#else` |
|     8707 |  281 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  282 | `#endif` |
|     4351 |  283 | `		}` |
|     8707 |  284 | `		return rVal;` |
|        7 |  285 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  286 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  287 | `		return 0;` |
|        - |  288 | `#else` |
|      ! 0 |  289 | `		return 0.0;` |
|        - |  290 | `#endif` |
|        7 |  291 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  292 | `		/* Return the total number of entries in the hashmap */` |
|      ! 0 |  293 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  294 | `		ph7_real n = (ph7_real)pMap->nEntry;` |
|      ! 0 |  295 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  296 | `		return n;` |
|        7 |  297 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  298 | `		ph7_value sResult;` |
|        5 |  299 | `		ph7_real rVal = 1;` |
|        - |  300 | `		sxi32 rc;` |
|        - |  301 | `		/* Invoke the [__toFloat()] magic method if available [note that this is a symisc extension]  */` |
|        5 |  302 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|        5 |  303 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  304 | `			"__toFloat",sizeof("__toFloat")-1,&sResult);` |
|        5 |  305 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_REAL) ){` |
|        - |  306 | `			/* Extract method return value */` |
|        5 |  307 | `			rVal = sResult.rVal;` |
|        2 |  308 | `		}` |
|        5 |  309 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        5 |  310 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  311 | `		return rVal;` |
|        3 |  312 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  313 | `		return (ph7_real)(pObj->x.pOther != 0);` |
|        - |  314 | `	}` |
|        - |  315 | `	/* NOT REACHED  */` |
|      ! 0 |  316 | `	return 0;` |
|     4743 |  317 | `}` |
|        - |  318 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  319 | `/*` |
|        - |  320 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  321 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  322 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  323 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  324 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  325 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  326 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  327 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  328 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  329 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  330 | ` */` |
|      514 |  331 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  332 | `{` |
|        - |  333 | `	sxi32 iExp,i;` |
|      518 |  334 | `	iExp = nLen - 1;` |
|     4416 |  335 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3902 |  336 | `		iExp--;` |
|        4 |  337 | `	}` |
|      518 |  338 | `	if( iExp <= 0 ){` |
|      472 |  339 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  340 | `	}` |
|        - |  341 | `	{` |
|       47 |  342 | `		sxi32 iDig = iExp + 1;` |
|        - |  343 | `		sxi32 iFirst;` |
|       47 |  344 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  345 | `			iDig++;` |
|       23 |  346 | `		}` |
|       47 |  347 | `		iFirst = iDig;` |
|       83 |  348 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  349 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  350 | `			iFirst++;` |
|        1 |  351 | `		}` |
|       47 |  352 | `		if( iFirst > iDig ){` |
|       25 |  353 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  354 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  355 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  356 | `			}` |
|       25 |  357 | `			nLen -= nStrip;` |
|       12 |  358 | `		}` |
|        - |  359 | `	}` |
|       47 |  360 | `	if( bGeneric ){` |
|       31 |  361 | `		int bHasDot = 0;` |
|       63 |  362 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  363 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  364 | `		}` |
|       31 |  365 | `		if( !bHasDot ){` |
|      107 |  366 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  367 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  368 | `			}` |
|       19 |  369 | `			zBuf[iExp] = '.';` |
|       19 |  370 | `			zBuf[iExp+1] = '0';` |
|       19 |  371 | `			nLen += 2;` |
|        9 |  372 | `		}` |
|       15 |  373 | `	}` |
|       47 |  374 | `	return nLen;` |
|      261 |  375 | `}` |
|        - |  376 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  377 | `/*` |
|        - |  378 | ` * Return the string representation of a given ph7_value.` |
|        - |  379 | ` * This function never fail and always return SXRET_OK.` |
|        - |  380 | ` */` |
|    61436 |  381 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  382 | `{` |
|    61441 |  383 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  384 | `		/* Handle special floating-point values first */` |
|      373 |  385 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  386 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      373 |  387 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  388 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  389 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  390 | `			}else{` |
|        5 |  391 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  392 | `			}` |
|        3 |  393 | `		}else{` |
|        - |  394 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  395 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  396 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  397 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  398 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  399 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  400 | `			 * exponent/fraction quirks. */` |
|        - |  401 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      369 |  402 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      369 |  403 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  404 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  405 | `			}` |
|      369 |  406 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      369 |  407 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  408 | `#else` |
|        - |  409 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  410 | `#endif` |
|        3 |  411 | `		}` |
|    61256 |  412 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    60765 |  413 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  414 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    30691 |  415 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       45 |  416 | `		if( bStrictBool ){` |
|        - |  417 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       45 |  418 | `			if( pObj->x.iVal ){` |
|       32 |  419 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       15 |  420 | `			}` |
|        - |  421 | `			/* false produces empty string, nothing to append */` |
|       25 |  422 | `		}else{` |
|        - |  423 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  424 | `			if( pObj->x.iVal ){` |
|      ! 0 |  425 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  426 | `			}else{` |
|      ! 0 |  427 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  428 | `			}` |
|        5 |  429 | `		}` |
|      291 |  430 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  431 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  432 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      271 |  433 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  434 | `		ph7_value sResult;` |
|        - |  435 | `		sxi32 rc;` |
|        - |  436 | `		/* Invoke the __toString() method if available */` |
|      179 |  437 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      179 |  438 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  439 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      179 |  440 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  441 | `			/* Expand method return value */` |
|      100 |  442 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       52 |  443 | `		}else{` |
|        - |  444 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       81 |  445 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  446 | `		}` |
|      179 |  447 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      179 |  448 | `		PH7_MemObjRelease(&sResult);` |
|      180 |  449 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        3 |  450 | `		SyBlobFormat(&(*pOut),"ResourceID_%#x",pObj->x.pOther);` |
|        1 |  451 | `	}` |
|    61441 |  452 | `	return SXRET_OK;` |
|        5 |  453 | `}` |
|        - |  454 | `/*` |
|        - |  455 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  456 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  457 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  458 | ` * (php's exact set):` |
|        - |  459 | ` * NULL` |
|        - |  460 | ` * the boolean FALSE itself.` |
|        - |  461 | ` * the integer 0 (zero).` |
|        - |  462 | ` * the real 0.0 (zero).` |
|        - |  463 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  464 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  465 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  466 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  467 | ` * an array with zero elements.` |
|        - |  468 | ` */` |
|    18428 |  469 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  470 | `{` |
|        - |  471 | `	sxi32 iFlags;` |
|    18433 |  472 | `	iFlags = pObj->iFlags;` |
|    18433 |  473 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  474 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  475 | `		return pObj->rVal ? 1 : 0;` |
|        - |  476 | `#else` |
|       14 |  477 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  478 | `#endif` |
|    18421 |  479 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      875 |  480 | `		return pObj->x.iVal ? 1 : 0;` |
|    17551 |  481 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  482 | `		SyString sString;` |
|      111 |  483 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  484 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|      111 |  485 | `		if( sString.nByte == 0 ){` |
|       19 |  486 | `			return 0;` |
|        - |  487 | `		}` |
|       94 |  488 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  489 | `			return 0;` |
|        - |  490 | `		}` |
|       88 |  491 | `		return 1;` |
|    17443 |  492 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    16155 |  493 | `		return 0;` |
|     1293 |  494 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       22 |  495 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       22 |  496 | `		sxu32 n = pMap->nEntry;` |
|       22 |  497 | `		PH7_HashmapUnref(pMap);` |
|       22 |  498 | `		return n > 0 ? TRUE : FALSE;` |
|     1273 |  499 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  500 | `		ph7_value sResult;` |
|      176 |  501 | `		sxi32 iVal = 1;` |
|        - |  502 | `		sxi32 rc;` |
|        - |  503 | `		/* Invoke the __toBool() method if available [note that this is a symisc extension]  */` |
|      176 |  504 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      176 |  505 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  506 | `			"__toBool",sizeof("__toBool")-1,&sResult);` |
|      176 |  507 | `		if( rc == SXRET_OK && (sResult.iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) ){` |
|        - |  508 | `			/* Extract method return value */` |
|        5 |  509 | `			iVal = (sxi32)(sResult.x.iVal != 0); /* Stupid cc warning -W -Wall -O6 */` |
|        2 |  510 | `		}` |
|      176 |  511 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      176 |  512 | `		PH7_MemObjRelease(&sResult);` |
|      176 |  513 | `		return iVal;` |
|     1099 |  514 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1099 |  515 | `		return pObj->x.pOther != 0;` |
|        - |  516 | `	}` |
|        - |  517 | `	/* NOT REACHED */` |
|      ! 0 |  518 | `	return 0;` |
|     9219 |  519 | `}` |
|        - |  520 | `/*` |
|        - |  521 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  522 | ` */` |
|    10648 |  523 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  524 | `{` |
|    10653 |  525 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  526 | `  /* Only mark the value as an integer if` |
|        - |  527 | `  **` |
|        - |  528 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  529 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  530 | `  **        possible integer` |
|        - |  531 | `  **` |
|        - |  532 | `  ** The second and third terms in the following conditional enforces` |
|        - |  533 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  534 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  535 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  536 | `  ** architectures might behave differently.` |
|        - |  537 | `  */` |
|    10648 |  538 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     9205 |  539 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     9189 |  540 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     4594 |  541 | `	}` |
|    10653 |  542 | `	return SXRET_OK;` |
|        5 |  543 | `}` |
|        - |  544 | `/*` |
|        - |  545 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  546 | ` */` |
|   523636 |  547 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  548 | `{` |
|   523641 |  549 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  550 | `		/* Preform the conversion */` |
|     1347 |  551 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  552 | `		/* Invalidate any prior representations */` |
|     1347 |  553 | `		SyBlobRelease(&pObj->sBlob);` |
|     1347 |  554 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      671 |  555 | `	}` |
|   523641 |  556 | `	return SXRET_OK;` |
|        5 |  557 | `}` |
|        - |  558 | `/*` |
|        - |  559 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  560 | ` * Invalidate any prior representations` |
|        - |  561 | ` */` |
|    10470 |  562 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  563 | `{` |
|    10475 |  564 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  565 | `		/* Preform the conversion */` |
|     9481 |  566 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  567 | `		/* Invalidate any prior representations */` |
|     9481 |  568 | `		SyBlobRelease(&pObj->sBlob);` |
|     9481 |  569 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  570 | `		/* Try to get an integer representation */` |
|     9481 |  571 | `		MemObjTryIntger(&(*pObj));` |
|     4738 |  572 | `	}` |
|    10475 |  573 | `	return SXRET_OK;` |
|        5 |  574 | `}` |
|        - |  575 | `/*` |
|        - |  576 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  577 | ` */` |
|    21940 |  578 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  579 | `{` |
|    21945 |  580 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  581 | `		/* Preform the conversion */` |
|    18433 |  582 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  583 | `		/* Invalidate any prior representations */` |
|    18433 |  584 | `		SyBlobRelease(&pObj->sBlob);` |
|    18433 |  585 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     9214 |  586 | `	}` |
|    21945 |  587 | `	return SXRET_OK;` |
|        5 |  588 | `}` |
|        - |  589 | `/*` |
|        - |  590 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  591 | ` */` |
|   987445 |  592 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  593 | `{` |
|   987450 |  594 | `	sxi32 rc = SXRET_OK;` |
|   987450 |  595 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  596 | `		/* Perform the conversion */` |
|    61347 |  597 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    61347 |  598 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    61347 |  599 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    30671 |  600 | `	}` |
|   987450 |  601 | `	return rc;` |
|        5 |  602 | `}` |
|        - |  603 | `/*` |
|        - |  604 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  605 | ` * representation.` |
|        - |  606 | ` */` |
|      ! 0 |  607 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  608 | `{` |
|      ! 0 |  609 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  610 | `}` |
|        - |  611 | `/*` |
|        - |  612 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  613 | `  * According to the PHP language reference manual.` |
|        - |  614 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  615 | `  *   to an array results in an array with a single element with index zero` |
|        - |  616 | `  *   and the value of the scalar which was converted.` |
|        - |  617 | `  */` |
|      550 |  618 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  619 | `{` |
|      555 |  620 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  621 | `		ph7_hashmap *pMap;` |
|        - |  622 | `		/* Allocate a new hashmap instance */` |
|      359 |  623 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      359 |  624 | `		if( pMap == 0 ){` |
|      ! 0 |  625 | `			return SXERR_MEM;` |
|        - |  626 | `		}` |
|      359 |  627 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  628 | `			/*` |
|        - |  629 | `			 * According to the PHP language reference manual.` |
|        - |  630 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  631 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  632 | `			 *   and the value of the scalar which was converted.` |
|        - |  633 | `			 */` |
|       29 |  634 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  635 | `				/* Object cast */` |
|       15 |  636 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        8 |  637 | `			}else{` |
|        - |  638 | `				/* Insert a single element */` |
|       15 |  639 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  640 | `			}` |
|       29 |  641 | `			SyBlobRelease(&pObj->sBlob);` |
|       14 |  642 | `		}` |
|        - |  643 | `		/* Invalidate any prior representation */` |
|      359 |  644 | `		PH7_MemObjRelease(pObj);` |
|      359 |  645 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      359 |  646 | `		pObj->x.pOther = pMap;` |
|      177 |  647 | `	}` |
|      555 |  648 | `	return SXRET_OK;` |
|      280 |  649 | `}` |
|        - |  650 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  651 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  652 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  653 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       38 |  654 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 |  655 | `{` |
|       39 |  656 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  657 | `	ph7_value *pSlot;` |
|        - |  658 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  659 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  660 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  661 | `	 * safe to coerce in place. */` |
|       39 |  662 | `	PH7_MemObjToString(pKey);` |
|       58 |  663 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       38 |  664 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       39 |  665 | `	if( pSlot ){` |
|       39 |  666 | `		PH7_MemObjStore(pValue,pSlot);` |
|       19 |  667 | `	}` |
|       39 |  668 | `	return SXRET_OK;` |
|        1 |  669 | `}` |
|        - |  670 | `/*` |
|        - |  671 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  672 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  673 | ` * matching PHP's (object) cast:` |
|        - |  674 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  675 | ` *   - scalar -> a single property named "scalar".` |
|        - |  676 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  677 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  678 | ` */` |
|       34 |  679 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        1 |  680 | `{` |
|       35 |  681 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  682 | `		ph7_class_instance *pStd;` |
|        - |  683 | `		ph7_class *pClass;` |
|        - |  684 | `		ph7_vm *pVm;` |
|        - |  685 | `		/* Point to the underlying VM + the stdClass */` |
|       35 |  686 | `		pVm = pObj->pVm;` |
|       52 |  687 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       17 |  688 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       35 |  689 | `		if( pClass == 0 ){` |
|        - |  690 | `			/* Can't happen,load null instead */` |
|      ! 0 |  691 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  692 | `			return SXRET_OK;` |
|        - |  693 | `		}` |
|        - |  694 | `		/* Instanciate a new (empty) stdClass object */` |
|       35 |  695 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       35 |  696 | `		if( pStd == 0 ){` |
|        - |  697 | `			/* Out of memory */` |
|      ! 0 |  698 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  699 | `			return SXRET_OK;` |
|        - |  700 | `		}` |
|       35 |  701 | `		pStd->iRef = 1;` |
|       35 |  702 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  703 | `			/* Array: one dynamic property per entry. */` |
|        - |  704 | `			struct VmObjCastData sData;` |
|       23 |  705 | `			sData.pVm = pVm;` |
|       23 |  706 | `			sData.pStd = pStd;` |
|       23 |  707 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       24 |  708 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  709 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  710 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  711 | `			if( pSlot ){` |
|       11 |  712 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  713 | `			}` |
|        5 |  714 | `		}` |
|        - |  715 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  716 | `		/* Invalidate any prior representation */` |
|       35 |  717 | `		PH7_MemObjRelease(pObj);` |
|        - |  718 | `		/* Save the new instance */` |
|       35 |  719 | `		pObj->x.pOther = pStd;` |
|       35 |  720 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       17 |  721 | `	}` |
|       35 |  722 | `	return SXRET_OK;` |
|       18 |  723 | `}` |
|        - |  724 | `/*` |
|        - |  725 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  726 | ` * with the given type.` |
|        - |  727 | ` * Note on type juggling.` |
|        - |  728 | ` * Accoding to the PHP language reference manual` |
|        - |  729 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  730 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  731 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  732 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  733 | ` *  assigned to $var, it becomes an integer.` |
|        - |  734 | ` */` |
|       84 |  735 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  736 | `{` |
|       89 |  737 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  738 | `		return PH7_MemObjToString;` |
|       75 |  739 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       59 |  740 | `		return PH7_MemObjToInteger;` |
|       20 |  741 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       17 |  742 | `		return PH7_MemObjToReal;` |
|        3 |  743 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  744 | `		return PH7_MemObjToBool;` |
|        3 |  745 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  746 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  747 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  748 | `		return PH7_MemObjToObject;` |
|      ! 0 |  749 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  750 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  751 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  752 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  753 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  754 | `		 * default. */` |
|      ! 0 |  755 | `		return 0;` |
|        - |  756 | `	}` |
|        - |  757 | `	/* NULL cast */` |
|      ! 0 |  758 | `	return PH7_MemObjToNull;` |
|       47 |  759 | `}` |
|        - |  760 | `/*` |
|        - |  761 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  762 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  763 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  764 | ` * loose-comparison numeric gate:` |
|        - |  765 | ` *` |
|        - |  766 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  767 | ` *` |
|        - |  768 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  769 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  770 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  771 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  772 | ` * a non-string value.` |
|        - |  773 | ` */` |
|        - |  774 | `/*` |
|        - |  775 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - |  776 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - |  777 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - |  778 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - |  779 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - |  780 | ` * and rejects a string with no prefix outright.` |
|        - |  781 | ` */` |
|   248355 |  782 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 |  783 | `{` |
|        - |  784 | `	const char *z, *zEnd;` |
|        - |  785 | `	sxu32 n;` |
|   248360 |  786 | `	int bDigit = 0;` |
|   248360 |  787 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  788 | `		return 0;` |
|        - |  789 | `	}` |
|   248360 |  790 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   248360 |  791 | `	n = SyBlobLength(&pValue->sBlob);` |
|   248360 |  792 | `	if( n == 0 ){` |
|      602 |  793 | `		return 0;` |
|        - |  794 | `	}` |
|   247760 |  795 | `	zEnd = z + n;` |
|   247786 |  796 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       28 |  797 | `		z++;` |
|        2 |  798 | `	}` |
|   247760 |  799 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      203 |  800 | `		z++;` |
|       99 |  801 | `	}` |
|   252570 |  802 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     4814 |  803 | `		z++; bDigit = 1;` |
|        4 |  804 | `	}` |
|   247760 |  805 | `	if( z < zEnd && z[0] == '.' ){` |
|     1733 |  806 | `		z++;` |
|     1803 |  807 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       73 |  808 | `			z++; bDigit = 1;` |
|        3 |  809 | `		}` |
|      864 |  810 | `	}` |
|        - |  811 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   247760 |  812 | `	if( !bDigit ){` |
|   243052 |  813 | `		return 0;` |
|        - |  814 | `	}` |
|        - |  815 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - |  816 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|     4712 |  817 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       28 |  818 | `		const char *zExp = z;` |
|       28 |  819 | `		z++;` |
|       28 |  820 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  821 | `			z++;` |
|      ! 0 |  822 | `		}` |
|       28 |  823 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       10 |  824 | `			z = zExp;` |
|        6 |  825 | `		}else{` |
|       42 |  826 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       24 |  827 | `				z++;` |
|        2 |  828 | `			}` |
|        - |  829 | `		}` |
|       13 |  830 | `	}` |
|     4712 |  831 | `	if( pzTail ){` |
|     4712 |  832 | `		*pzTail = z;` |
|     2354 |  833 | `	}` |
|     4712 |  834 | `	return 1;` |
|   124186 |  835 | `}` |
|        - |  836 | `/*` |
|        - |  837 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - |  838 | ` * (trailing whitespace allowed, nothing else).` |
|        - |  839 | ` */` |
|   245969 |  840 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  841 | `{` |
|   245974 |  842 | `	const char *zTail = 0, *zEnd;` |
|   245974 |  843 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   243642 |  844 | `		return 0;` |
|        - |  845 | `	}` |
|     2335 |  846 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|     2341 |  847 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        8 |  848 | `		zTail++;` |
|        2 |  849 | `	}` |
|     2335 |  850 | `	return zTail == zEnd ? 1 : 0;` |
|   122993 |  851 | `}` |
|        - |  852 | `/*` |
|        - |  853 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  854 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  855 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  856 | ` */` |
|   246817 |  857 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  858 | `{` |
|   246822 |  859 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      597 |  860 | `		return TRUE;` |
|   246230 |  861 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      374 |  862 | `		return FALSE;` |
|   245858 |  863 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  864 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   245858 |  865 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  866 | `	}` |
|        - |  867 | `	/* NOT REACHED */` |
|      ! 0 |  868 | `	return FALSE;` |
|   123417 |  869 | `}` |
|        - |  870 | `/*` |
|        - |  871 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  872 | ` * FALSE otherwise.` |
|        - |  873 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  874 | ` * NULL value.` |
|        - |  875 | ` * Boolean FALSE.` |
|        - |  876 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  877 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  878 | ` * An empty array.` |
|        - |  879 | ` * NOTE` |
|        - |  880 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  881 | ` */` |
|    40634 |  882 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  883 | `{` |
|    40639 |  884 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       21 |  885 | `		return TRUE;` |
|    40621 |  886 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  887 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    40601 |  888 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  889 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    40601 |  890 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  891 | `		return !pObj->x.iVal;` |
|    40597 |  892 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    27315 |  893 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    21787 |  894 | `			return TRUE;` |
|      ! 0 |  895 | `		}else{` |
|        - |  896 | `			const char *zIn,*zEnd;` |
|     5533 |  897 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5533 |  898 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5539 |  899 | `			while( zIn < zEnd ){` |
|     5539 |  900 | `				if( zIn[0] != '0' ){` |
|     5533 |  901 | `					break;` |
|        - |  902 | `				}` |
|        7 |  903 | `				zIn++;` |
|        1 |  904 | `			}` |
|     5533 |  905 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  906 | `		}` |
|    13287 |  907 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|    13287 |  908 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|    13287 |  909 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  910 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  911 | `		return FALSE;` |
|        - |  912 | `	}` |
|        - |  913 | `	/* Assume empty by default */` |
|      ! 0 |  914 | `	return TRUE;` |
|    20322 |  915 | `}` |
|        - |  916 | `/*` |
|        - |  917 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  918 | ` * or both.` |
|        - |  919 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  920 | ` * the conversion, even if the input is a string that does not look` |
|        - |  921 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  922 | ` * and ignore the rest.` |
|        - |  923 | ` */` |
|   594761 |  924 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  925 | `{` |
|   594766 |  926 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   592428 |  927 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        5 |  928 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        3 |  929 | `				pObj->x.iVal = 0;` |
|        1 |  930 | `			}` |
|        5 |  931 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        2 |  932 | `		}` |
|        - |  933 | `		/* Already numeric */` |
|   592428 |  934 | `		return  SXRET_OK;` |
|        - |  935 | `	}` |
|     2341 |  936 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     2341 |  937 | `		const char *zTail = 0;` |
|     2341 |  938 | `		int bNum, bReal = 0;` |
|        - |  939 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - |  940 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - |  941 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - |  942 | `		 * php sees the prefix "1" there and yields int(1). */` |
|     2341 |  943 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|     2341 |  944 | `		if( bNum ){` |
|     2341 |  945 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     4757 |  946 | `			while( z < zTail ){` |
|     2441 |  947 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       23 |  948 | `					bReal = 1;` |
|       23 |  949 | `					break;` |
|        - |  950 | `				}` |
|     2419 |  951 | `				z++;` |
|        3 |  952 | `			}` |
|     1169 |  953 | `		}` |
|     2341 |  954 | `		if( bReal ){` |
|       23 |  955 | `			PH7_MemObjToReal(&(*pObj));` |
|       12 |  956 | `		}else{` |
|     2319 |  957 | `			if( !bNum ){` |
|        - |  958 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  959 | `				pObj->x.iVal = 0;` |
|      ! 0 |  960 | `			}else{` |
|        - |  961 | `				/* Convert as much as we can */` |
|     2319 |  962 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  963 | `			}` |
|     2319 |  964 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|     2319 |  965 | `			SyBlobRelease(&pObj->sBlob);` |
|        3 |  966 | `		}` |
|     1169 |  967 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  968 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  969 | `	}else{` |
|        - |  970 | `		/* Perform a blind cast */` |
|      ! 0 |  971 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  972 | `	}` |
|     2341 |  973 | `	return SXRET_OK;` |
|   297409 |  974 | `}` |
|        - |  975 | `/*` |
|        - |  976 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  977 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  978 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  979 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  980 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  981 | ` * last carried character. Empty strings become "1".` |
|        - |  982 | ` *` |
|        - |  983 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  984 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  985 | ` * a string even though it looks numeric.` |
|        - |  986 | ` */` |
|       50 |  987 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|        2 |  988 | `{` |
|        - |  989 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       52 |  990 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  991 | `	sxu32 nLen, pos;` |
|        - |  992 | `	sxu8 *zStr;` |
|       52 |  993 | `	int carry = 1;` |
|        - |  994 | `	int ch;` |
|        - |  995 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  996 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  997 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  998 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  999 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       52 | 1000 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       48 | 1001 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       23 | 1002 | `	}` |
|       52 | 1003 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       52 | 1004 | `	if( nLen == 0 ){` |
|        5 | 1005 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|        5 | 1006 | `		return SXRET_OK;` |
|        - | 1007 | `	}` |
|       48 | 1008 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       48 | 1009 | `	pos = nLen;` |
|      100 | 1010 | `	while( pos > 0 ){` |
|       82 | 1011 | `		pos--;` |
|       82 | 1012 | `		ch = zStr[pos];` |
|       82 | 1013 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       48 | 1014 | `			if( ch == 'z' ){` |
|       29 | 1015 | `				zStr[pos] = 'a';` |
|       29 | 1016 | `				last_class = CARRY_LOWER;` |
|       29 | 1017 | `				continue;` |
|        - | 1018 | `			}` |
|       20 | 1019 | `			zStr[pos]++;` |
|       20 | 1020 | `			carry = 0;` |
|       20 | 1021 | `			break;` |
|       35 | 1022 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       21 | 1023 | `			if( ch == 'Z' ){` |
|       19 | 1024 | `				zStr[pos] = 'A';` |
|       19 | 1025 | `				last_class = CARRY_UPPER;` |
|       19 | 1026 | `				continue;` |
|        - | 1027 | `			}` |
|        3 | 1028 | `			zStr[pos]++;` |
|        3 | 1029 | `			carry = 0;` |
|        3 | 1030 | `			break;` |
|       15 | 1031 | `		}else if( ch >= '0' && ch <= '9' ){` |
|        7 | 1032 | `			if( ch == '9' ){` |
|        7 | 1033 | `				zStr[pos] = '0';` |
|        7 | 1034 | `				last_class = CARRY_DIGIT;` |
|        7 | 1035 | `				continue;` |
|        - | 1036 | `			}` |
|      ! 0 | 1037 | `			zStr[pos]++;` |
|      ! 0 | 1038 | `			carry = 0;` |
|      ! 0 | 1039 | `			break;` |
|      ! 0 | 1040 | `		}else{` |
|        - | 1041 | `			/* non-alphanumeric: stop without prepending */` |
|        9 | 1042 | `			carry = 0;` |
|        9 | 1043 | `			break;` |
|        - | 1044 | `		}` |
|      ! 0 | 1045 | `	}` |
|       48 | 1046 | `	if( carry ){` |
|        - | 1047 | `		sxu8 prepend;` |
|        - | 1048 | `		sxu32 i;` |
|       19 | 1049 | `		switch( last_class ){` |
|        9 | 1050 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       11 | 1051 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1052 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1053 | `		}` |
|        - | 1054 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       19 | 1055 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       19 | 1056 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       19 | 1057 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1058 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       57 | 1059 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       39 | 1060 | `			zStr[i] = zStr[i - 1];` |
|       20 | 1061 | `		}` |
|       19 | 1062 | `		zStr[0] = prepend;` |
|        9 | 1063 | `	}` |
|       48 | 1064 | `	return SXRET_OK;` |
|       27 | 1065 | `}` |
|        - | 1066 | `/*` |
|        - | 1067 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1068 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1069 | ` */` |
|     1104 | 1070 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        4 | 1071 | `{` |
|     1108 | 1072 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1073 | `		/* Work only with reals */` |
|     1108 | 1074 | `		MemObjTryIntger(&(*pObj));` |
|      552 | 1075 | `	}` |
|     1108 | 1076 | `	return SXRET_OK;` |
|        4 | 1077 | `}` |
|        - | 1078 | `/*` |
|        - | 1079 | ` * Initialize a ph7_value to the null type.` |
|        - | 1080 | ` */` |
| 24831603 | 1081 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1082 | `{` |
|        - | 1083 | `	/* Zero the structure */` |
| 24831608 | 1084 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1085 | `	/* Initialize fields */` |
| 24831608 | 1086 | `	pObj->pVm = pVm;` |
| 24831608 | 1087 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1088 | `	/* Set the NULL type */` |
| 24831608 | 1089 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 24831608 | 1090 | `	return SXRET_OK;` |
|        5 | 1091 | `}` |
|        - | 1092 | `/*` |
|        - | 1093 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1094 | ` */` |
|  5702550 | 1095 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1096 | `{` |
|        - | 1097 | `	/* Zero the structure */` |
|  5702555 | 1098 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1099 | `	/* Initialize fields */` |
|  5702555 | 1100 | `	pObj->pVm = pVm;` |
|  5702555 | 1101 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1102 | `	/* Set the desired type */` |
|  5702555 | 1103 | `	pObj->x.iVal = iVal;` |
|  5702555 | 1104 | `	pObj->iFlags = MEMOBJ_INT;` |
|  5702555 | 1105 | `	return SXRET_OK;` |
|        5 | 1106 | `}` |
|        - | 1107 | `/*` |
|        - | 1108 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1109 | ` */` |
|    17250 | 1110 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1111 | `{` |
|        - | 1112 | `	/* Zero the structure */` |
|    17255 | 1113 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1114 | `	/* Initialize fields */` |
|    17255 | 1115 | `	pObj->pVm = pVm;` |
|    17255 | 1116 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1117 | `	/* Set the desired type */` |
|    17255 | 1118 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    17255 | 1119 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    17255 | 1120 | `	return SXRET_OK;` |
|        5 | 1121 | `}` |
|        - | 1122 | `/*` |
|        - | 1123 | ` * Initialize a ph7_value to the real type.` |
|        - | 1124 | ` */` |
|       10 | 1125 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1126 | `{` |
|        - | 1127 | `	/* Zero the structure */` |
|       11 | 1128 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1129 | `	/* Initialize fields */` |
|       11 | 1130 | `	pObj->pVm = pVm;` |
|       11 | 1131 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1132 | `	/* Set the desired type */` |
|       11 | 1133 | `	pObj->rVal = rVal;` |
|       11 | 1134 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1135 | `	return SXRET_OK;` |
|        1 | 1136 | `}` |
|        - | 1137 | `/*` |
|        - | 1138 | ` * Initialize a ph7_value to the array type.` |
|        - | 1139 | ` */` |
|    62940 | 1140 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1141 | `{` |
|        - | 1142 | `	/* Zero the structure */` |
|    62945 | 1143 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1144 | `	/* Initialize fields */` |
|    62945 | 1145 | `	pObj->pVm = pVm;` |
|    62945 | 1146 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1147 | `	/* Set the desired type */` |
|    62945 | 1148 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    62945 | 1149 | `	pObj->x.pOther = pArray;` |
|    62945 | 1150 | `	return SXRET_OK;` |
|        5 | 1151 | `}` |
|        - | 1152 | `/*` |
|        - | 1153 | ` * Initialize a ph7_value to the string type.` |
|        - | 1154 | ` */` |
|  4154400 | 1155 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1156 | `{` |
|        - | 1157 | `	/* Zero the structure */` |
|  4154405 | 1158 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1159 | `	/* Initialize fields */` |
|  4154405 | 1160 | `	pObj->pVm = pVm;` |
|  4154405 | 1161 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  4154405 | 1162 | `	if( pVal ){` |
|        - | 1163 | `		/* Append contents */` |
|  1940209 | 1164 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   970102 | 1165 | `	}` |
|        - | 1166 | `	/* Set the desired type */` |
|  4154405 | 1167 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  4154405 | 1168 | `	return SXRET_OK;` |
|        5 | 1169 | `}` |
|        - | 1170 | `/*` |
|        - | 1171 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1172 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1173 | ` * invalidate any prior representation and set the string type.` |
|        - | 1174 | ` * Then a simple append operation is performed.` |
|        - | 1175 | ` */` |
|  2613524 | 1176 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1177 | `{` |
|        - | 1178 | `	sxi32 rc;` |
|  2613529 | 1179 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1180 | `		/* Invalidate any prior representation */` |
|     3499 | 1181 | `		PH7_MemObjRelease(pObj);` |
|     3499 | 1182 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1747 | 1183 | `	}` |
|        - | 1184 | `	/* Append contents */` |
|  2613529 | 1185 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  2613529 | 1186 | `	return rc;` |
|        5 | 1187 | `}` |
|        - | 1188 | `#if 0` |
|        - | 1189 | `/*` |
|        - | 1190 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1191 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1192 | ` * any prior representation and set the string type.` |
|        - | 1193 | ` * Then a simple format and append operation is performed.` |
|        - | 1194 | ` */` |
|        - | 1195 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1196 | `{` |
|        - | 1197 | `	sxi32 rc;` |
|        - | 1198 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1199 | `		/* Invalidate any prior representation */` |
|        - | 1200 | `		PH7_MemObjRelease(pObj);` |
|        - | 1201 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1202 | `	}` |
|        - | 1203 | `	/* Format and append contents */` |
|        - | 1204 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1205 | `	return rc;` |
|        - | 1206 | `}` |
|        - | 1207 | `#endif` |
|        - | 1208 | `/*` |
|        - | 1209 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1210 | ` */` |
|  5952261 | 1211 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1212 | `{` |
|  5952266 | 1213 | `	ph7_class_instance *pObj = 0;` |
|  5952266 | 1214 | `	ph7_hashmap *pMap = 0;` |
|        - | 1215 | `	sxi32 rc;` |
|  5952266 | 1216 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1217 | `		/* Increment reference count */` |
|   216801 | 1218 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5843868 | 1219 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1220 | `		/* Increment reference count */` |
|     9701 | 1221 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     4848 | 1222 | `	}` |
|  5952266 | 1223 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    80467 | 1224 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5912035 | 1225 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8287 | 1226 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4141 | 1227 | `	}` |
|  5952266 | 1228 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  5952266 | 1229 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  5952266 | 1230 | `	rc = SXRET_OK;` |
|  5952266 | 1231 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4251361 | 1232 | `		SyBlobReset(&pDest->sBlob);` |
|  4251361 | 1233 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2125683 | 1234 | `	}else{` |
|  1700910 | 1235 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   357028 | 1236 | `			SyBlobRelease(&pDest->sBlob);` |
|   178535 | 1237 | `		}` |
|        - | 1238 | `	}` |
|  5952266 | 1239 | `	if( pMap ){` |
|    80467 | 1240 | `		PH7_HashmapUnref(pMap);` |
|  5912035 | 1241 | `	}else if( pObj ){` |
|     8287 | 1242 | `		PH7_ClassInstanceUnref(pObj);` |
|     4141 | 1243 | `	}` |
|  5952261 | 1244 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  3084552 | 1245 | `	 && pDest->pVm` |
|   216796 | 1246 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1247 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1248 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1249 | `	  * for closure envs and other non-slot destinations. */` |
|   108407 | 1250 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1251 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1252 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1253 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1254 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1255 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1256 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1257 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1258 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1259 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1260 | `			pDest->x.pOther = pSnap;` |
|        4 | 1261 | `		}else if( pSnap ){` |
|      ! 0 | 1262 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1263 | `		}` |
|        4 | 1264 | `	}` |
|  5952266 | 1265 | `	return rc;` |
|        5 | 1266 | `}` |
|        - | 1267 | `/*` |
|        - | 1268 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1269 | ` * buffer contents,simply point to it.` |
|        - | 1270 | ` */` |
|  8594746 | 1271 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1272 | `{` |
|  8594751 | 1273 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1274 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  8594751 | 1275 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1276 | `		/* Increment reference count */` |
|   536927 | 1277 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  8326290 | 1278 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1279 | `		/* Increment reference count */` |
|    47497 | 1280 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    23746 | 1281 | `	}` |
|  8594751 | 1282 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       46 | 1283 | `		SyBlobRelease(&pDest->sBlob);` |
|       21 | 1284 | `	}` |
|  8594751 | 1285 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4455011 | 1286 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  2227557 | 1287 | `	}` |
|  8594751 | 1288 | `	return SXRET_OK;` |
|        5 | 1289 | `}` |
|        - | 1290 | `/*` |
|        - | 1291 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1292 | ` */` |
| 20625655 | 1293 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1294 | `{` |
| 20625660 | 1295 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 17991459 | 1296 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   701283 | 1297 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 17640820 | 1298 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|    89301 | 1299 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    44648 | 1300 | `		}` |
|        - | 1301 | `		/* Release the internal buffer */` |
| 17991459 | 1302 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1303 | `		/* Invalidate any prior representation */` |
| 17991459 | 1304 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  8995918 | 1305 | `	}` |
| 20625660 | 1306 | `	return SXRET_OK;` |
|        5 | 1307 | `}` |
|        - | 1308 | `/*` |
|        - | 1309 | ` * Compare two ph7_values.` |
|        - | 1310 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1311 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1312 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1313 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1314 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1315 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1316 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1317 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1318 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1319 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1320 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1321 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1322 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1323 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1324 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1325 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1326 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1327 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1328 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1329 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1330 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1331 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1332 | ` *      Loose comparisons with ==` |
|        - | 1333 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1334 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1335 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1336 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1337 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1338 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1339 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1340 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1341 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1342 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1343 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1344 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1345 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1346 | ` *    Strict comparisons with ===` |
|        - | 1347 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1348 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1349 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1350 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1351 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1352 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1353 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1354 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1355 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1356 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1357 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1358 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1359 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1360 | ` */` |
|  1657308 | 1361 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1362 | `{` |
|        - | 1363 | `	sxi32 iComb;` |
|        - | 1364 | `	sxi32 rc;` |
|  1657313 | 1365 | `	if( bStrict ){` |
|        - | 1366 | `		sxi32 iF1,iF2;` |
|        - | 1367 | `		/* Strict comparisons with === */` |
|   848164 | 1368 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   848164 | 1369 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   848164 | 1370 | `		if( iF1 != iF2 ){` |
|        - | 1371 | `			/* Not of the same type */` |
|   193483 | 1372 | `			return 1;` |
|        - | 1373 | `		}` |
|   327340 | 1374 | `	}` |
|        - | 1375 | `	/* Combine flag together */` |
|  1463835 | 1376 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1463830 | 1377 | `	if( !bStrict` |
|  1136489 | 1378 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   404639 | 1379 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       66 | 1380 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1381 | `		/*` |
|        - | 1382 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1383 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1384 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1385 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1386 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1387 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1388 | `		 */` |
|       45 | 1389 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1390 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1391 | `		}else{` |
|       11 | 1392 | `			PH7_MemObjToString(pObj2);` |
|        - | 1393 | `		}` |
|       45 | 1394 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1395 | `	}` |
|  1463835 | 1396 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1397 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    24373 | 1398 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     9137 | 1399 | `			PH7_MemObjToBool(pObj1);` |
|     4566 | 1400 | `		}` |
|    24373 | 1401 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     8089 | 1402 | `			PH7_MemObjToBool(pObj2);` |
|     4042 | 1403 | `		}` |
|    24373 | 1404 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1439467 | 1405 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1406 | `		/* Hashmap aka 'array' comparison */` |
|       31 | 1407 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1408 | `			/* Array is always greater */` |
|      ! 0 | 1409 | `			return -1;` |
|        - | 1410 | `		}` |
|       31 | 1411 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1412 | `			/* Array is always greater */` |
|      ! 0 | 1413 | `			return 1;` |
|        - | 1414 | `		}` |
|        - | 1415 | `		/* Perform the comparison */` |
|       31 | 1416 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       31 | 1417 | `		return rc;` |
|  1439437 | 1418 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1419 | `		/* Object comparison */` |
|      277 | 1420 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1421 | `			/* Object is always greater */` |
|      ! 0 | 1422 | `			return -1;` |
|        - | 1423 | `		}` |
|      277 | 1424 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1425 | `			/* Object is always greater */` |
|      ! 0 | 1426 | `			return 1;` |
|        - | 1427 | `		}` |
|        - | 1428 | `		/* Perform the comparison */` |
|      277 | 1429 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      277 | 1430 | `		return rc;` |
|  1439165 | 1431 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1432 | `		SyString s1,s2;` |
|   871880 | 1433 | `		if( !bStrict ){` |
|        - | 1434 | `			/*` |
|        - | 1435 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1436 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1437 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1438 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1439 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1440 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1441 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1442 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1443 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1444 | `			 * below, unchanged.` |
|        - | 1445 | `			 */` |
|   244370 | 1446 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1447 | `				/* Perform a numeric comparison */` |
|     1099 | 1448 | `				goto Numeric;` |
|        - | 1449 | `			}` |
|   121637 | 1450 | `		}` |
|        - | 1451 | `		/* Perform a strict string comparison.*/` |
|   870782 | 1452 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       23 | 1453 | `			PH7_MemObjToString(pObj1);` |
|       11 | 1454 | `		}` |
|   870782 | 1455 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        7 | 1456 | `			PH7_MemObjToString(pObj2);` |
|        3 | 1457 | `		}` |
|   870782 | 1458 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   870782 | 1459 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1460 | `		/*` |
|        - | 1461 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1462 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1463 | `		 */` |
|   870782 | 1464 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   870782 | 1465 | `		if( rc == 0 ){` |
|   288544 | 1466 | `			if( s1.nByte != s2.nByte ){` |
|    17340 | 1467 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     8669 | 1468 | `			}` |
|   144271 | 1469 | `		}` |
|   870782 | 1470 | `		return rc;` |
|   567290 | 1471 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   283621 | 1472 | `Numeric:` |
|        - | 1473 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   568388 | 1474 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1081 | 1475 | `			PH7_MemObjToNumeric(pObj1);` |
|      540 | 1476 | `		}` |
|   568388 | 1477 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1087 | 1478 | `			PH7_MemObjToNumeric(pObj2);` |
|      543 | 1479 | `		}` |
|   568388 | 1480 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1481 | `			/*` |
|        - | 1482 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1483 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1484 | `			 */` |
|        - | 1485 | `			ph7_real r1,r2;` |
|        - | 1486 | `			/* Compare as reals */` |
|      314 | 1487 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1488 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1489 | `			}` |
|      314 | 1490 | `			r1 = pObj1->rVal;` |
|      314 | 1491 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 | 1492 | `				PH7_MemObjToReal(pObj2);` |
|       27 | 1493 | `			}` |
|      314 | 1494 | `			r2 = pObj2->rVal;` |
|      314 | 1495 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1496 | `				/*` |
|        - | 1497 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1498 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1499 | `				 * any non-NaN numeric value.` |
|        - | 1500 | `				 */` |
|       50 | 1501 | `				if( PH7_IS_NAN(r1) ){` |
|       40 | 1502 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1503 | `				}` |
|       11 | 1504 | `				return -1;` |
|        - | 1505 | `			}` |
|      266 | 1506 | `			if( r1 > r2 ){` |
|       54 | 1507 | `				return 1;` |
|      214 | 1508 | `			}else if( r1 < r2 ){` |
|      134 | 1509 | `				return -1;` |
|        - | 1510 | `			}` |
|       82 | 1511 | `			return 0;` |
|      ! 0 | 1512 | `		}else{` |
|        - | 1513 | `			/* Integer comparison */` |
|   568076 | 1514 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     7499 | 1515 | `				return 1;` |
|   560582 | 1516 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   549406 | 1517 | `				return -1;` |
|        - | 1518 | `			}` |
|    11181 | 1519 | `			return 0;` |
|        - | 1520 | `		}` |
|        - | 1521 | `	}` |
|        - | 1522 | `	/* NOT REACHED */` |
|      ! 0 | 1523 | `	return 0;` |
|   828684 | 1524 | `}` |
|        - | 1525 | `/*` |
|        - | 1526 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1527 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1528 | ` * is that the '+' operator is overloaded.` |
|        - | 1529 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1530 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1531 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1532 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1533 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1534 | ` * be ignored.` |
|        - | 1535 | ` * This function take care of handling all the scenarios.` |
|        - | 1536 | ` */` |
|    12934 | 1537 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1538 | `{` |
|    12939 | 1539 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1540 | `			/* Arithemtic operation */` |
|     9165 | 1541 | `			PH7_MemObjToNumeric(pObj1);` |
|     9165 | 1542 | `			PH7_MemObjToNumeric(pObj2);` |
|     9165 | 1543 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1544 | `				/* Floating point arithmetic */` |
|        - | 1545 | `				ph7_real a,b;` |
|       69 | 1546 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       29 | 1547 | `					PH7_MemObjToReal(pObj1);` |
|       14 | 1548 | `				}` |
|       69 | 1549 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 1550 | `					PH7_MemObjToReal(pObj2);` |
|        4 | 1551 | `				}` |
|       69 | 1552 | `				a = pObj1->rVal;` |
|       69 | 1553 | `				b = pObj2->rVal;` |
|       69 | 1554 | `				pObj1->rVal = a+b;` |
|       69 | 1555 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1556 | `				/* Try to get an integer representation also */` |
|       69 | 1557 | `				MemObjTryIntger(&(*pObj1));` |
|       35 | 1558 | `			}else{` |
|        - | 1559 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1560 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1561 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1562 | `				sxi64 a,b,r;` |
|     9097 | 1563 | `				a = pObj1->x.iVal;` |
|     9097 | 1564 | `				b = pObj2->x.iVal;` |
|     9097 | 1565 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1566 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1567 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1568 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1569 | `#else` |
|        - | 1570 | `					pObj1->x.iVal = r;` |
|        - | 1571 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1572 | `#endif` |
|        5 | 1573 | `				}else{` |
|     9089 | 1574 | `					pObj1->x.iVal = r;` |
|     9089 | 1575 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1576 | `				}` |
|        - | 1577 | `			}` |
|     4585 | 1578 | `	}else{` |
|     3779 | 1579 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1580 | `			ph7_hashmap *pMap;` |
|        - | 1581 | `			sxi32 rc;` |
|     3779 | 1582 | `			if( bAddStore ){` |
|        - | 1583 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1584 | `				 */` |
|        3 | 1585 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1586 | `					/* Force a hashmap cast */` |
|      ! 0 | 1587 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1588 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1589 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1590 | `						return rc;` |
|        - | 1591 | `					}` |
|      ! 0 | 1592 | `				}` |
|        - | 1593 | `				/* COW separate before in-place mutation */` |
|        3 | 1594 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1595 | `			}else{` |
|        - | 1596 | `				/* Create a new hashmap */` |
|     3777 | 1597 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3777 | 1598 | `				if( pMap == 0){` |
|      ! 0 | 1599 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1600 | `					return SXERR_MEM;` |
|        - | 1601 | `				}` |
|        - | 1602 | `			}` |
|     3779 | 1603 | `			if( !bAddStore ){` |
|     3777 | 1604 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1605 | `					/* Perform a hashmap duplication */` |
|     3777 | 1606 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1891 | 1607 | `				}else{` |
|      ! 0 | 1608 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1609 | `						/* Simple insertion */` |
|      ! 0 | 1610 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1611 | `					}` |
|        - | 1612 | `				}` |
|     1886 | 1613 | `			}` |
|        - | 1614 | `			/* Perform the union */` |
|     3779 | 1615 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3779 | 1616 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1892 | 1617 | `			}else{` |
|      ! 0 | 1618 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1619 | `					/* Simple insertion */` |
|      ! 0 | 1620 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1621 | `				}` |
|        - | 1622 | `			}` |
|        - | 1623 | `			/* Reflect the change */` |
|     3779 | 1624 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1625 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1626 | `			}` |
|     3779 | 1627 | `			pObj1->x.pOther = pMap;` |
|     3779 | 1628 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1887 | 1629 | `		}` |
|        - | 1630 | `	}` |
|    12939 | 1631 | `	return SXRET_OK;` |
|     6472 | 1632 | `}` |
|        - | 1633 | `/*` |
|        - | 1634 | ` * Return a printable representation of the type of a given` |
|        - | 1635 | ` * ph7_value.` |
|        - | 1636 | ` */` |
|      ! 0 | 1637 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|      ! 0 | 1638 | `{` |
|      ! 0 | 1639 | `	const char *zType = "";` |
|      ! 0 | 1640 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1641 | `		zType = "null";` |
|      ! 0 | 1642 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1643 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1644 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 1645 | `		zType = "double";` |
|      ! 0 | 1646 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 1647 | `		zType = "int";` |
|      ! 0 | 1648 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1649 | `		zType = "string";` |
|      ! 0 | 1650 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1651 | `		zType = "bool";` |
|      ! 0 | 1652 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 1653 | `		zType = "array";` |
|      ! 0 | 1654 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1655 | `		zType = "object";` |
|      ! 0 | 1656 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1657 | `		zType = "resource";` |
|      ! 0 | 1658 | `	}` |
|      ! 0 | 1659 | `	return zType;` |
|      ! 0 | 1660 | `}` |
|        - | 1661 | `/*` |
|        - | 1662 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1663 | ` * Store the dump in the given blob.` |
|        - | 1664 | ` */` |
|        - | 1665 | `/*` |
|        - | 1666 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1667 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1668 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1669 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1670 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1671 | ` */` |
|        4 | 1672 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1673 | `{` |
|        5 | 1674 | `	if( PH7_IS_NAN(rVal) ){` |
|      ! 0 | 1675 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|      ! 0 | 1676 | `		return;` |
|        - | 1677 | `	}` |
|        5 | 1678 | `	if( PH7_IS_INF(rVal) ){` |
|      ! 0 | 1679 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|      ! 0 | 1680 | `		return;` |
|        - | 1681 | `	}` |
|        - | 1682 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1683 | `	{` |
|        - | 1684 | `		char zNum[48];` |
|        5 | 1685 | `		sxi32 n = 0;` |
|        - | 1686 | `		int p;` |
|        7 | 1687 | `		for( p = 1 ; p <= 17 ; p++ ){` |
|        7 | 1688 | `			n = (sxi32)snprintf(zNum,sizeof(zNum),"%.*G",p,rVal);` |
|        7 | 1689 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 | 1690 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 | 1691 | `			}` |
|        7 | 1692 | `			if( strtod(zNum,0) == rVal ){` |
|        5 | 1693 | `				break; /* shortest round-trip found */` |
|        - | 1694 | `			}` |
|        2 | 1695 | `		}` |
|        5 | 1696 | `		n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|        5 | 1697 | `		SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - | 1698 | `	}` |
|        - | 1699 | `#else` |
|        - | 1700 | `	SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1701 | `#endif` |
|        3 | 1702 | `}` |
|        - | 1703 | `/*` |
|        - | 1704 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1705 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1706 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1707 | ` */` |
|      218 | 1708 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1709 | `{` |
|      220 | 1710 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        7 | 1711 | `		return;` |
|        - | 1712 | `	}` |
|      214 | 1713 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1714 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1715 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1716 | `		}` |
|      ! 0 | 1717 | `		return;` |
|        - | 1718 | `	}` |
|      214 | 1719 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1720 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1721 | `		 * non-strings into the output) */` |
|      120 | 1722 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      120 | 1723 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       59 | 1724 | `		}` |
|      120 | 1725 | `		return;` |
|        - | 1726 | `	}` |
|       96 | 1727 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      111 | 1728 | `}` |
|      584 | 1729 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1730 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1731 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1732 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1733 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1734 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1735 | `	int nDepth,        /* Nesting level */` |
|        - | 1736 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1737 | `	)` |
|        4 | 1738 | `{` |
|      588 | 1739 | `	sxi32 rc = SXRET_OK;` |
|        - | 1740 | `	int i;` |
|      588 | 1741 | `	if( !ShowType ){` |
|        - | 1742 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1743 | `		 * containers render the Array/Object block (which the container` |
|        - | 1744 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      112 | 1745 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      105 | 1746 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1747 | `		}` |
|        8 | 1748 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1749 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1750 | `		}` |
|        3 | 1751 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1752 | `		return SXRET_OK;` |
|        - | 1753 | `	}` |
|        - | 1754 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1755 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1756 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     4562 | 1757 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4087 | 1758 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2045 | 1759 | `	}` |
|      478 | 1760 | `	if( isRef ){` |
|        7 | 1761 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        3 | 1762 | `	}` |
|      478 | 1763 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1764 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1765 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1766 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1767 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1768 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1769 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1770 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1771 | `			}` |
|      ! 0 | 1772 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1773 | `			return SXRET_OK;` |
|        - | 1774 | `		}` |
|      139 | 1775 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1776 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1777 | `		return rc;` |
|        - | 1778 | `	}` |
|      342 | 1779 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        3 | 1780 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|        3 | 1781 | `		return SXRET_OK;` |
|        - | 1782 | `	}` |
|      340 | 1783 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       24 | 1784 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       24 | 1785 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       24 | 1786 | `		return rc;` |
|        - | 1787 | `	}` |
|      318 | 1788 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      125 | 1789 | `		if( pObj->x.iVal != 0 ){` |
|       81 | 1790 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       42 | 1791 | `		}else{` |
|       47 | 1792 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1793 | `		}` |
|      125 | 1794 | `		return SXRET_OK;` |
|        - | 1795 | `	}` |
|      195 | 1796 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1797 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1798 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        5 | 1799 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        5 | 1800 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        5 | 1801 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        5 | 1802 | `		return SXRET_OK;` |
|        - | 1803 | `	}` |
|      191 | 1804 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      131 | 1805 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      131 | 1806 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      131 | 1807 | `		return SXRET_OK;` |
|        - | 1808 | `	}` |
|       63 | 1809 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       63 | 1810 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|       63 | 1811 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       57 | 1812 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       27 | 1813 | `		}` |
|       63 | 1814 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|       63 | 1815 | `		return SXRET_OK;` |
|        - | 1816 | `	}` |
|        - | 1817 | ``	/* Resources and anything else: the legacy `type(value)` shape (php's`` |
|        - | 1818 | ``	 * `resource(N) of type (stream)` needs the §8 typed-resource model). */`` |
|        - | 1819 | `	{` |
|      ! 0 | 1820 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1821 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1822 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1823 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1824 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1825 | `	}` |
|      ! 0 | 1826 | `	return rc;` |
|      296 | 1827 | `}` |
|        - | 1828 |  |
