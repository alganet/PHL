# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 819/1003 lines (81.66%)

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
|      366 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |   66 | `{` |
|      371 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      333 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|      325 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      239 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      229 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       24 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        3 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|      ! 0 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |   75 | `	return "unknown";` |
|      188 |   76 | `}` |
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
|    10780 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|    10785 |  111 | `  ph7_real r = pObj->rVal;` |
|    10785 |  112 | `  if( r<(ph7_real)minInt ){` |
|        3 |  113 | `    return minInt;` |
|    10783 |  114 | `  }else if( r>(ph7_real)maxInt ){` |
|        - |  115 | `    /* minInt is correct here - not maxInt.  It turns out that assigning` |
|        - |  116 | `    ** a very large positive number to an integer results in a very large` |
|        - |  117 | `    ** negative integer.  This makes no sense, but it is what x86 hardware` |
|        - |  118 | `    ** does so for compatibility we will do the same in software. */` |
|      184 |  119 | `    return minInt;` |
|      ! 0 |  120 | `  }else{` |
|    10601 |  121 | `    return (sxi64)r;` |
|        - |  122 | `  }` |
|        - |  123 | `#endif` |
|     5395 |  124 | `}` |
|        - |  125 | `/*` |
|        - |  126 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  127 | ` * to a 64-bit integer.` |
|        - |  128 | ` */` |
|  3801190 |  129 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  130 | `{` |
|  3801195 |  131 | `	sxi64 iVal = 0;` |
|  3801195 |  132 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  133 | `		return 0;` |
|        - |  134 | `	}` |
|  3801195 |  135 | `	if( pVal->zString[0] == '0' ){` |
|        - |  136 | `		sxi32 c;` |
|  1494399 |  137 | `		if( pVal->nByte == sizeof(char) ){` |
|  1385739 |  138 | `			return 0;` |
|        - |  139 | `		}` |
|   108665 |  140 | `		c = pVal->zString[1];` |
|   108665 |  141 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  142 | `			/* Hex digit stream */` |
|   104423 |  143 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|    56456 |  144 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  145 | `			/* Binary digit stream */` |
|      285 |  146 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     4105 |  147 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|        - |  148 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|        - |  149 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|       21 |  150 | `			if( pVal->nByte > 2 ){` |
|       21 |  151 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|       10 |  152 | `			}` |
|       11 |  153 | `		}else{` |
|        - |  154 | `			/* Legacy octal digit stream (leading 0) */` |
|     3943 |  155 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  156 | `		}` |
|    54335 |  157 | `	}else{` |
|        - |  158 | `		/* Decimal digit stream */` |
|  2306801 |  159 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  160 | `	}` |
|  2415461 |  161 | `	return iVal;` |
|  1900600 |  162 | `}` |
|        - |  163 | `/*` |
|        - |  164 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  165 | ` * do at representing the value that pObj describes as a string` |
|        - |  166 | ` * representation.` |
|        - |  167 | ` */` |
|     3707 |  168 | `static sxi64 MemObjStringToInt(ph7_value *pObj)` |
|        5 |  169 | `{` |
|     3712 |  170 | `	sxi64 iVal = 0;` |
|        - |  171 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|        - |  172 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|        - |  173 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|     3712 |  174 | `	SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0);` |
|     3712 |  175 | `	return iVal;` |
|        5 |  176 | `}` |
|        - |  177 | `/*` |
|        - |  178 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  179 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  180 | ` * successfully called. Any other return value indicates failure.` |
|        - |  181 | ` */` |
|      174 |  182 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  183 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  184 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  185 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  186 | `	sxu32 nLen,                /* Method name length */` |
|        - |  187 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  188 | `	)` |
|        4 |  189 | `{` |
|        - |  190 | `	ph7_class_method *pMethod;` |
|        - |  191 | `	/* Check if the method is available */` |
|      178 |  192 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      178 |  193 | `	if( pMethod == 0 ){` |
|        - |  194 | `		/* No such method */` |
|        3 |  195 | `		return SXERR_NOTFOUND;` |
|        - |  196 | `	}` |
|        - |  197 | `	/* Invoke the desired method */` |
|      176 |  198 | `	PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|        - |  199 | `	/* Method successfully called,pResult should hold the return value */` |
|      176 |  200 | `	return SXRET_OK;` |
|       91 |  201 | `}` |
|        - |  202 | `/*` |
|        - |  203 | ` * Return some kind of integer value which is the best we can` |
|        - |  204 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  205 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  206 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  207 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  208 | ` * a integer and return that.` |
|        - |  209 | ` * If pObj represents a NULL value, return 0.` |
|        - |  210 | ` */` |
|     1566 |  211 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  212 | `{` |
|        - |  213 | `	sxi32 iFlags;` |
|     1571 |  214 | `	iFlags = pObj->iFlags;` |
|     1571 |  215 | `	if (iFlags & MEMOBJ_REAL ){` |
|       23 |  216 | `		return MemObjRealToInt(&(*pObj));` |
|     1551 |  217 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      168 |  218 | `		return pObj->x.iVal;` |
|     1385 |  219 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|     1367 |  220 | `		return MemObjStringToInt(&(*pObj));` |
|       19 |  221 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        5 |  222 | `		return 0;` |
|       15 |  223 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  224 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|        - |  225 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|        - |  226 | `		 * followed php; int/float did not.) */` |
|        7 |  227 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        7 |  228 | `		sxu32 n = pMap->nEntry;` |
|        7 |  229 | `		PH7_HashmapUnref(pMap);` |
|        7 |  230 | `		return n > 0 ? 1 : 0;` |
|        9 |  231 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  232 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|        - |  233 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|        - |  234 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|        7 |  235 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        7 |  236 | `		if( pInst && pInst->pClass ){` |
|       10 |  237 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        6 |  238 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|        3 |  239 | `		}` |
|        7 |  240 | `		PH7_ClassInstanceUnref(pInst);` |
|        7 |  241 | `		return 1;` |
|        3 |  242 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        - |  243 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|        - |  244 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        3 |  245 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  246 | `	}` |
|        - |  247 | `	/* CANT HAPPEN */` |
|      ! 0 |  248 | `	return 0;` |
|      788 |  249 | `}` |
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
|     9606 |  260 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  261 | `{` |
|        - |  262 | `	sxi32 iFlags;` |
|     9611 |  263 | `	iFlags = pObj->iFlags;` |
|     9611 |  264 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  265 | `		return pObj->rVal;` |
|     9611 |  266 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      771 |  267 | `		return (ph7_real)pObj->x.iVal;` |
|     8843 |  268 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  269 | `		SyString sString;` |
|        - |  270 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  271 | `		ph7_real rVal = 0;` |
|        - |  272 | `#else` |
|     8835 |  273 | `		ph7_real rVal = 0.0;` |
|        - |  274 | `#endif` |
|     8835 |  275 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     8835 |  276 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  277 | `			/* Convert as much as we can */` |
|        - |  278 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  279 | `			rVal = MemObjStringToInt(&(*pObj));` |
|        - |  280 | `#else` |
|     8835 |  281 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  282 | `#endif` |
|     4415 |  283 | `		}` |
|     8835 |  284 | `		return rVal;` |
|        9 |  285 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  286 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  287 | `		return 0;` |
|        - |  288 | `#else` |
|      ! 0 |  289 | `		return 0.0;` |
|        - |  290 | `#endif` |
|        9 |  291 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  292 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|        - |  293 | `		 * branch above. */` |
|      ! 0 |  294 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  295 | `		sxu32 n = pMap->nEntry;` |
|      ! 0 |  296 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  297 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        9 |  298 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  299 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        7 |  300 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        7 |  301 | `		if( pInst && pInst->pClass ){` |
|       10 |  302 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        6 |  303 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|        3 |  304 | `		}` |
|        7 |  305 | `		PH7_ClassInstanceUnref(pInst);` |
|        7 |  306 | `		return (ph7_real)1.0;` |
|        3 |  307 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  308 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  309 | `	}` |
|        - |  310 | `	/* NOT REACHED  */` |
|      ! 0 |  311 | `	return 0;` |
|     4808 |  312 | `}` |
|        - |  313 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  314 | `/*` |
|        - |  315 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  316 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  317 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  318 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  319 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  320 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  321 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  322 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  323 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  324 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  325 | ` */` |
|      508 |  326 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        5 |  327 | `{` |
|        - |  328 | `	sxi32 iExp,i;` |
|      513 |  329 | `	iExp = nLen - 1;` |
|     4403 |  330 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     3895 |  331 | `		iExp--;` |
|        5 |  332 | `	}` |
|      513 |  333 | `	if( iExp <= 0 ){` |
|      467 |  334 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  335 | `	}` |
|        - |  336 | `	{` |
|       47 |  337 | `		sxi32 iDig = iExp + 1;` |
|        - |  338 | `		sxi32 iFirst;` |
|       47 |  339 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       47 |  340 | `			iDig++;` |
|       23 |  341 | `		}` |
|       47 |  342 | `		iFirst = iDig;` |
|       83 |  343 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       61 |  344 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       25 |  345 | `			iFirst++;` |
|        1 |  346 | `		}` |
|       47 |  347 | `		if( iFirst > iDig ){` |
|       25 |  348 | `			sxi32 nStrip = iFirst - iDig;` |
|       73 |  349 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       49 |  350 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       25 |  351 | `			}` |
|       25 |  352 | `			nLen -= nStrip;` |
|       12 |  353 | `		}` |
|        - |  354 | `	}` |
|       47 |  355 | `	if( bGeneric ){` |
|       31 |  356 | `		int bHasDot = 0;` |
|       63 |  357 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       45 |  358 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       17 |  359 | `		}` |
|       31 |  360 | `		if( !bHasDot ){` |
|      107 |  361 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       89 |  362 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       45 |  363 | `			}` |
|       19 |  364 | `			zBuf[iExp] = '.';` |
|       19 |  365 | `			zBuf[iExp+1] = '0';` |
|       19 |  366 | `			nLen += 2;` |
|        9 |  367 | `		}` |
|       15 |  368 | `	}` |
|       47 |  369 | `	return nLen;` |
|      259 |  370 | `}` |
|        - |  371 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  372 | `/*` |
|        - |  373 | ` * Return the string representation of a given ph7_value.` |
|        - |  374 | ` * This function never fail and always return SXRET_OK.` |
|        - |  375 | ` */` |
|    62244 |  376 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  377 | `{` |
|    62249 |  378 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  379 | `		/* Handle special floating-point values first */` |
|      372 |  380 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|      ! 0 |  381 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      372 |  382 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        5 |  383 | `			if( pObj->rVal < 0.0 ){` |
|      ! 0 |  384 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|      ! 0 |  385 | `			}else{` |
|        5 |  386 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  387 | `			}` |
|        3 |  388 | `		}else{` |
|        - |  389 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  390 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  391 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  392 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  393 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  394 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  395 | `			 * exponent/fraction quirks. */` |
|        - |  396 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      368 |  397 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      368 |  398 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  399 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  400 | `			}` |
|      368 |  401 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      368 |  402 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  403 | `#else` |
|        - |  404 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  405 | `#endif` |
|        4 |  406 | `		}` |
|    62065 |  407 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    61593 |  408 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  409 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    31087 |  410 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       47 |  411 | `		if( bStrictBool ){` |
|        - |  412 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       47 |  413 | `			if( pObj->x.iVal ){` |
|       34 |  414 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       16 |  415 | `			}` |
|        - |  416 | `			/* false produces empty string, nothing to append */` |
|       26 |  417 | `		}else{` |
|        - |  418 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  419 | `			if( pObj->x.iVal ){` |
|      ! 0 |  420 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  421 | `			}else{` |
|      ! 0 |  422 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  423 | `			}` |
|        5 |  424 | `		}` |
|      272 |  425 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 |  426 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      ! 0 |  427 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      251 |  428 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  429 | `		ph7_value sResult;` |
|        - |  430 | `		sxi32 rc;` |
|        - |  431 | `		/* Invoke the __toString() method if available */` |
|      178 |  432 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      178 |  433 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  434 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      178 |  435 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) && SyBlobLength(&sResult.sBlob) > 0){` |
|        - |  436 | `			/* Expand method return value */` |
|      100 |  437 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       52 |  438 | `		}else{` |
|        - |  439 | `			/* Expand "Object" as requested by the PHP language reference manual */` |
|       80 |  440 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  441 | `		}` |
|      178 |  442 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      178 |  443 | `		PH7_MemObjRelease(&sResult);` |
|      161 |  444 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        - |  445 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|        - |  446 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|        5 |  447 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|        2 |  448 | `	}` |
|    62249 |  449 | `	return SXRET_OK;` |
|        5 |  450 | `}` |
|        - |  451 | `/*` |
|        - |  452 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  453 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  454 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  455 | ` * (php's exact set):` |
|        - |  456 | ` * NULL` |
|        - |  457 | ` * the boolean FALSE itself.` |
|        - |  458 | ` * the integer 0 (zero).` |
|        - |  459 | ` * the real 0.0 (zero).` |
|        - |  460 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  461 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  462 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  463 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  464 | ` * an array with zero elements.` |
|        - |  465 | ` */` |
|    46848 |  466 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  467 | `{` |
|        - |  468 | `	sxi32 iFlags;` |
|    46853 |  469 | `	iFlags = pObj->iFlags;` |
|    46853 |  470 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  471 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  472 | `		return pObj->rVal ? 1 : 0;` |
|        - |  473 | `#else` |
|       14 |  474 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  475 | `#endif` |
|    46841 |  476 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      939 |  477 | `		return pObj->x.iVal ? 1 : 0;` |
|    45907 |  478 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  479 | `		SyString sString;` |
|       91 |  480 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  481 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|       91 |  482 | `		if( sString.nByte == 0 ){` |
|       19 |  483 | `			return 0;` |
|        - |  484 | `		}` |
|       74 |  485 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        7 |  486 | `			return 0;` |
|        - |  487 | `		}` |
|       68 |  488 | `		return 1;` |
|    45819 |  489 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    44515 |  490 | `		return 0;` |
|     1309 |  491 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  492 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       20 |  493 | `		sxu32 n = pMap->nEntry;` |
|       20 |  494 | `		PH7_HashmapUnref(pMap);` |
|       20 |  495 | `		return n > 0 ? TRUE : FALSE;` |
|     1291 |  496 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  497 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|        - |  498 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|        - |  499 | `		 * extension changed control flow in valid php source. */` |
|      196 |  500 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      196 |  501 | `		return 1;` |
|     1097 |  502 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1097 |  503 | `		return pObj->x.pOther != 0;` |
|        - |  504 | `	}` |
|        - |  505 | `	/* NOT REACHED */` |
|      ! 0 |  506 | `	return 0;` |
|    23429 |  507 | `}` |
|        - |  508 | `/*` |
|        - |  509 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  510 | ` */` |
|    10760 |  511 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  512 | `{` |
|    10765 |  513 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  514 | `  /* Only mark the value as an integer if` |
|        - |  515 | `  **` |
|        - |  516 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  517 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  518 | `  **        possible integer` |
|        - |  519 | `  **` |
|        - |  520 | `  ** The second and third terms in the following conditional enforces` |
|        - |  521 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  522 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  523 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  524 | `  ** architectures might behave differently.` |
|        - |  525 | `  */` |
|    10760 |  526 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     9317 |  527 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     9301 |  528 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     4650 |  529 | `	}` |
|    10765 |  530 | `	return SXRET_OK;` |
|        5 |  531 | `}` |
|        - |  532 | `/*` |
|        - |  533 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  534 | ` */` |
|   533494 |  535 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  536 | `{` |
|   533499 |  537 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  538 | `		/* Preform the conversion */` |
|     1571 |  539 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  540 | `		/* Invalidate any prior representations */` |
|     1571 |  541 | `		SyBlobRelease(&pObj->sBlob);` |
|     1571 |  542 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      783 |  543 | `	}` |
|   533499 |  544 | `	return SXRET_OK;` |
|        5 |  545 | `}` |
|        - |  546 | `/*` |
|        - |  547 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  548 | ` * Invalidate any prior representations` |
|        - |  549 | ` */` |
|    10598 |  550 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  551 | `{` |
|    10603 |  552 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  553 | `		/* Preform the conversion */` |
|     9611 |  554 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  555 | `		/* Invalidate any prior representations */` |
|     9611 |  556 | `		SyBlobRelease(&pObj->sBlob);` |
|     9611 |  557 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  558 | `		/* Try to get an integer representation */` |
|     9611 |  559 | `		MemObjTryIntger(&(*pObj));` |
|     4803 |  560 | `	}` |
|    10603 |  561 | `	return SXRET_OK;` |
|        5 |  562 | `}` |
|        - |  563 | `/*` |
|        - |  564 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  565 | ` */` |
|    51060 |  566 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  567 | `{` |
|    51065 |  568 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  569 | `		/* Preform the conversion */` |
|    46853 |  570 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  571 | `		/* Invalidate any prior representations */` |
|    46853 |  572 | `		SyBlobRelease(&pObj->sBlob);` |
|    46853 |  573 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    23424 |  574 | `	}` |
|    51065 |  575 | `	return SXRET_OK;` |
|        5 |  576 | `}` |
|        - |  577 | `/*` |
|        - |  578 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  579 | ` */` |
|   965347 |  580 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  581 | `{` |
|   965352 |  582 | `	sxi32 rc = SXRET_OK;` |
|   965352 |  583 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  584 | `		/* Perform the conversion */` |
|    62149 |  585 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    62149 |  586 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    62149 |  587 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    31072 |  588 | `	}` |
|   965352 |  589 | `	return rc;` |
|        5 |  590 | `}` |
|        - |  591 | `/*` |
|        - |  592 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - |  593 | ` * representation.` |
|        - |  594 | ` */` |
|      ! 0 |  595 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|      ! 0 |  596 | `{` |
|      ! 0 |  597 | `	return PH7_MemObjRelease(pObj);` |
|      ! 0 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - |  601 | `  * According to the PHP language reference manual.` |
|        - |  602 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  603 | `  *   to an array results in an array with a single element with index zero` |
|        - |  604 | `  *   and the value of the scalar which was converted.` |
|        - |  605 | `  */` |
|      570 |  606 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 |  607 | `{` |
|      575 |  608 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  609 | `		ph7_hashmap *pMap;` |
|        - |  610 | `		/* Allocate a new hashmap instance */` |
|      377 |  611 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      377 |  612 | `		if( pMap == 0 ){` |
|      ! 0 |  613 | `			return SXERR_MEM;` |
|        - |  614 | `		}` |
|      377 |  615 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - |  616 | `			/*` |
|        - |  617 | `			 * According to the PHP language reference manual.` |
|        - |  618 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - |  619 | `			 *   to an array results in an array with a single element with index zero` |
|        - |  620 | `			 *   and the value of the scalar which was converted.` |
|        - |  621 | `			 */` |
|       34 |  622 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  623 | `				/* Object cast */` |
|       22 |  624 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|       12 |  625 | `			}else{` |
|        - |  626 | `				/* Insert a single element */` |
|       13 |  627 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - |  628 | `			}` |
|       34 |  629 | `			SyBlobRelease(&pObj->sBlob);` |
|       16 |  630 | `		}` |
|        - |  631 | `		/* Invalidate any prior representation */` |
|      377 |  632 | `		PH7_MemObjRelease(pObj);` |
|      377 |  633 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      377 |  634 | `		pObj->x.pOther = pMap;` |
|      186 |  635 | `	}` |
|      575 |  636 | `	return SXRET_OK;` |
|      290 |  637 | `}` |
|        - |  638 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - |  639 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - |  640 | ` * matching PHP) and holding a copy of the value. */` |
|        - |  641 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       62 |  642 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        3 |  643 | `{` |
|       65 |  644 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - |  645 | `	ph7_value *pSlot;` |
|        - |  646 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - |  647 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - |  648 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - |  649 | `	 * safe to coerce in place. */` |
|       65 |  650 | `	PH7_MemObjToString(pKey);` |
|       96 |  651 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       62 |  652 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       65 |  653 | `	if( pSlot ){` |
|       65 |  654 | `		PH7_MemObjStore(pValue,pSlot);` |
|       31 |  655 | `	}` |
|       65 |  656 | `	return SXRET_OK;` |
|        3 |  657 | `}` |
|        - |  658 | `/*` |
|        - |  659 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - |  660 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - |  661 | ` * matching PHP's (object) cast:` |
|        - |  662 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - |  663 | ` *   - scalar -> a single property named "scalar".` |
|        - |  664 | ` *   - null   -> an empty stdClass (no properties).` |
|        - |  665 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - |  666 | ` */` |
|       46 |  667 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        3 |  668 | `{` |
|       49 |  669 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  670 | `		ph7_class_instance *pStd;` |
|        - |  671 | `		ph7_class *pClass;` |
|        - |  672 | `		ph7_vm *pVm;` |
|        - |  673 | `		/* Point to the underlying VM + the stdClass */` |
|       49 |  674 | `		pVm = pObj->pVm;` |
|       72 |  675 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       23 |  676 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       49 |  677 | `		if( pClass == 0 ){` |
|        - |  678 | `			/* Can't happen,load null instead */` |
|      ! 0 |  679 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  680 | `			return SXRET_OK;` |
|        - |  681 | `		}` |
|        - |  682 | `		/* Instanciate a new (empty) stdClass object */` |
|       49 |  683 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       49 |  684 | `		if( pStd == 0 ){` |
|        - |  685 | `			/* Out of memory */` |
|      ! 0 |  686 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 |  687 | `			return SXRET_OK;` |
|        - |  688 | `		}` |
|       49 |  689 | `		pStd->iRef = 1;` |
|       49 |  690 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  691 | `			/* Array: one dynamic property per entry. */` |
|        - |  692 | `			struct VmObjCastData sData;` |
|       37 |  693 | `			sData.pVm = pVm;` |
|       37 |  694 | `			sData.pStd = pStd;` |
|       37 |  695 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       30 |  696 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  697 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       11 |  698 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       11 |  699 | `			if( pSlot ){` |
|       11 |  700 | `				PH7_MemObjStore(pObj,pSlot);` |
|        5 |  701 | `			}` |
|        5 |  702 | `		}` |
|        - |  703 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - |  704 | `		/* Invalidate any prior representation */` |
|       49 |  705 | `		PH7_MemObjRelease(pObj);` |
|        - |  706 | `		/* Save the new instance */` |
|       49 |  707 | `		pObj->x.pOther = pStd;` |
|       49 |  708 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       23 |  709 | `	}` |
|       49 |  710 | `	return SXRET_OK;` |
|       26 |  711 | `}` |
|        - |  712 | `/*` |
|        - |  713 | ` * Return a pointer to the appropriate convertion method associated` |
|        - |  714 | ` * with the given type.` |
|        - |  715 | ` * Note on type juggling.` |
|        - |  716 | ` * Accoding to the PHP language reference manual` |
|        - |  717 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - |  718 | ` *  declaration; a variable's type is determined by the context in which` |
|        - |  719 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - |  720 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - |  721 | ` *  assigned to $var, it becomes an integer.` |
|        - |  722 | ` */` |
|       82 |  723 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 |  724 | `{` |
|       87 |  725 | `	if( iFlags & MEMOBJ_STRING ){` |
|       16 |  726 | `		return PH7_MemObjToString;` |
|       73 |  727 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       57 |  728 | `		return PH7_MemObjToInteger;` |
|       20 |  729 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       17 |  730 | `		return PH7_MemObjToReal;` |
|        3 |  731 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  732 | `		return PH7_MemObjToBool;` |
|        3 |  733 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        3 |  734 | `		return PH7_MemObjToHashmap;` |
|      ! 0 |  735 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  736 | `		return PH7_MemObjToObject;` |
|      ! 0 |  737 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  738 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - |  739 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - |  740 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - |  741 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - |  742 | `		 * default. */` |
|      ! 0 |  743 | `		return 0;` |
|        - |  744 | `	}` |
|        - |  745 | `	/* NULL cast */` |
|      ! 0 |  746 | `	return PH7_MemObjToNull;` |
|       46 |  747 | `}` |
|        - |  748 | `/*` |
|        - |  749 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - |  750 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - |  751 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - |  752 | ` * loose-comparison numeric gate:` |
|        - |  753 | ` *` |
|        - |  754 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - |  755 | ` *` |
|        - |  756 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - |  757 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - |  758 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - |  759 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - |  760 | ` * a non-string value.` |
|        - |  761 | ` */` |
|        - |  762 | `/*` |
|        - |  763 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - |  764 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - |  765 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - |  766 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - |  767 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - |  768 | ` * and rejects a string with no prefix outright.` |
|        - |  769 | ` */` |
|   249447 |  770 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 |  771 | `{` |
|        - |  772 | `	const char *z, *zEnd;` |
|        - |  773 | `	sxu32 n;` |
|   249452 |  774 | `	int bDigit = 0;` |
|   249452 |  775 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  776 | `		return 0;` |
|        - |  777 | `	}` |
|   249452 |  778 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   249452 |  779 | `	n = SyBlobLength(&pValue->sBlob);` |
|   249452 |  780 | `	if( n == 0 ){` |
|      603 |  781 | `		return 0;` |
|        - |  782 | `	}` |
|   248852 |  783 | `	zEnd = z + n;` |
|   248876 |  784 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       26 |  785 | `		z++;` |
|        2 |  786 | `	}` |
|   248852 |  787 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      215 |  788 | `		z++;` |
|      105 |  789 | `	}` |
|   253715 |  790 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     4867 |  791 | `		z++; bDigit = 1;` |
|        4 |  792 | `	}` |
|   248852 |  793 | `	if( z < zEnd && z[0] == '.' ){` |
|     6030 |  794 | `		z++;` |
|     6104 |  795 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       77 |  796 | `			z++; bDigit = 1;` |
|        3 |  797 | `		}` |
|     3218 |  798 | `	}` |
|        - |  799 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   248852 |  800 | `	if( !bDigit ){` |
|   244097 |  801 | `		return 0;` |
|        - |  802 | `	}` |
|        - |  803 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - |  804 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|     4759 |  805 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       30 |  806 | `		const char *zExp = z;` |
|       30 |  807 | `		z++;` |
|       30 |  808 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      ! 0 |  809 | `			z++;` |
|      ! 0 |  810 | `		}` |
|       30 |  811 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       10 |  812 | `			z = zExp;` |
|        6 |  813 | `		}else{` |
|       46 |  814 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       26 |  815 | `				z++;` |
|        2 |  816 | `			}` |
|        - |  817 | `		}` |
|       14 |  818 | `	}` |
|     4759 |  819 | `	if( pzTail ){` |
|     4759 |  820 | `		*pzTail = z;` |
|     2377 |  821 | `	}` |
|     4759 |  822 | `	return 1;` |
|   124599 |  823 | `}` |
|        - |  824 | `/*` |
|        - |  825 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - |  826 | ` * (trailing whitespace allowed, nothing else).` |
|        - |  827 | ` */` |
|   247034 |  828 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 |  829 | `{` |
|   247039 |  830 | `	const char *zTail = 0, *zEnd;` |
|   247039 |  831 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   244687 |  832 | `		return 0;` |
|        - |  833 | `	}` |
|     2356 |  834 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|     2362 |  835 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        8 |  836 | `		zTail++;` |
|        2 |  837 | `	}` |
|     2356 |  838 | `	return zTail == zEnd ? 1 : 0;` |
|   123393 |  839 | `}` |
|        - |  840 | `/*` |
|        - |  841 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - |  842 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - |  843 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - |  844 | ` */` |
|   248026 |  845 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 |  846 | `{` |
|   248031 |  847 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      683 |  848 | `		return TRUE;` |
|   247353 |  849 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      435 |  850 | `		return FALSE;` |
|   246921 |  851 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - |  852 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   246921 |  853 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - |  854 | `	}` |
|        - |  855 | `	/* NOT REACHED */` |
|      ! 0 |  856 | `	return FALSE;` |
|   123889 |  857 | `}` |
|        - |  858 | `/*` |
|        - |  859 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - |  860 | ` * FALSE otherwise.` |
|        - |  861 | ` * An ph7_value is considered empty if the following are true:` |
|        - |  862 | ` * NULL value.` |
|        - |  863 | ` * Boolean FALSE.` |
|        - |  864 | ` * Integer/Float with a 0 (zero) value.` |
|        - |  865 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - |  866 | ` * An empty array.` |
|        - |  867 | ` * NOTE` |
|        - |  868 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - |  869 | ` */` |
|    39392 |  870 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 |  871 | `{` |
|    39397 |  872 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       21 |  873 | `		return TRUE;` |
|    39379 |  874 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|       22 |  875 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|    39359 |  876 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  877 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|    39359 |  878 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        5 |  879 | `		return !pObj->x.iVal;` |
|    39355 |  880 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    26217 |  881 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|    20869 |  882 | `			return TRUE;` |
|      ! 0 |  883 | `		}else{` |
|        - |  884 | `			const char *zIn,*zEnd;` |
|     5353 |  885 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|     5353 |  886 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|     5359 |  887 | `			while( zIn < zEnd ){` |
|     5359 |  888 | `				if( zIn[0] != '0' ){` |
|     5353 |  889 | `					break;` |
|        - |  890 | `				}` |
|        7 |  891 | `				zIn++;` |
|        1 |  892 | `			}` |
|     5353 |  893 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|      ! 0 |  894 | `		}` |
|    13143 |  895 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|    13143 |  896 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|    13143 |  897 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|      ! 0 |  898 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      ! 0 |  899 | `		return FALSE;` |
|        - |  900 | `	}` |
|        - |  901 | `	/* Assume empty by default */` |
|      ! 0 |  902 | `	return TRUE;` |
|    19701 |  903 | `}` |
|        - |  904 | `/*` |
|        - |  905 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - |  906 | ` * or both.` |
|        - |  907 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - |  908 | ` * the conversion, even if the input is a string that does not look` |
|        - |  909 | ` * completely like a number.Convert as much of the string as we can` |
|        - |  910 | ` * and ignore the rest.` |
|        - |  911 | ` */` |
|   526060 |  912 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 |  913 | `{` |
|   526065 |  914 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   523700 |  915 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        8 |  916 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        5 |  917 | `				pObj->x.iVal = 0;` |
|        2 |  918 | `			}` |
|        8 |  919 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        3 |  920 | `		}` |
|        - |  921 | `		/* Already numeric */` |
|   523700 |  922 | `		return  SXRET_OK;` |
|        - |  923 | `	}` |
|     2369 |  924 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     2369 |  925 | `		const char *zTail = 0;` |
|     2369 |  926 | `		int bNum, bReal = 0;` |
|        - |  927 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - |  928 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - |  929 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - |  930 | `		 * php sees the prefix "1" there and yields int(1). */` |
|     2369 |  931 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|     2369 |  932 | `		if( bNum ){` |
|     2369 |  933 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     4820 |  934 | `			while( z < zTail ){` |
|     2475 |  935 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       22 |  936 | `					bReal = 1;` |
|       22 |  937 | `					break;` |
|        - |  938 | `				}` |
|     2455 |  939 | `				z++;` |
|        4 |  940 | `			}` |
|     1182 |  941 | `		}` |
|     2369 |  942 | `		if( bReal ){` |
|       22 |  943 | `			PH7_MemObjToReal(&(*pObj));` |
|       12 |  944 | `		}else{` |
|     2349 |  945 | `			if( !bNum ){` |
|        - |  946 | `				/* The input does not look at all like a number,set the value to 0 */` |
|      ! 0 |  947 | `				pObj->x.iVal = 0;` |
|      ! 0 |  948 | `			}else{` |
|        - |  949 | `				/* Convert as much as we can */` |
|     2349 |  950 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj));` |
|        - |  951 | `			}` |
|     2349 |  952 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|     2349 |  953 | `			SyBlobRelease(&pObj->sBlob);` |
|        4 |  954 | `		}` |
|     1182 |  955 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|      ! 0 |  956 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 |  957 | `	}else{` |
|        - |  958 | `		/* Perform a blind cast */` |
|      ! 0 |  959 | `		PH7_MemObjToReal(&(*pObj));` |
|        - |  960 | `	}` |
|     2369 |  961 | `	return SXRET_OK;` |
|   263253 |  962 | `}` |
|        - |  963 | `/*` |
|        - |  964 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - |  965 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - |  966 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - |  967 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - |  968 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - |  969 | ` * last carried character. Empty strings become "1".` |
|        - |  970 | ` *` |
|        - |  971 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - |  972 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - |  973 | ` * a string even though it looks numeric.` |
|        - |  974 | ` */` |
|      ! 0 |  975 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|      ! 0 |  976 | `{` |
|        - |  977 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|      ! 0 |  978 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - |  979 | `	sxu32 nLen, pos;` |
|        - |  980 | `	sxu8 *zStr;` |
|      ! 0 |  981 | `	int carry = 1;` |
|        - |  982 | `	int ch;` |
|        - |  983 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - |  984 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - |  985 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - |  986 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - |  987 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|      ! 0 |  988 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      ! 0 |  989 | `		SyBlobNullAppend(&pObj->sBlob);` |
|      ! 0 |  990 | `	}` |
|      ! 0 |  991 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|      ! 0 |  992 | `	if( nLen == 0 ){` |
|      ! 0 |  993 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|      ! 0 |  994 | `		return SXRET_OK;` |
|        - |  995 | `	}` |
|      ! 0 |  996 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 |  997 | `	pos = nLen;` |
|      ! 0 |  998 | `	while( pos > 0 ){` |
|      ! 0 |  999 | `		pos--;` |
|      ! 0 | 1000 | `		ch = zStr[pos];` |
|      ! 0 | 1001 | `		if( ch >= 'a' && ch <= 'z' ){` |
|      ! 0 | 1002 | `			if( ch == 'z' ){` |
|      ! 0 | 1003 | `				zStr[pos] = 'a';` |
|      ! 0 | 1004 | `				last_class = CARRY_LOWER;` |
|      ! 0 | 1005 | `				continue;` |
|        - | 1006 | `			}` |
|      ! 0 | 1007 | `			zStr[pos]++;` |
|      ! 0 | 1008 | `			carry = 0;` |
|      ! 0 | 1009 | `			break;` |
|      ! 0 | 1010 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|      ! 0 | 1011 | `			if( ch == 'Z' ){` |
|      ! 0 | 1012 | `				zStr[pos] = 'A';` |
|      ! 0 | 1013 | `				last_class = CARRY_UPPER;` |
|      ! 0 | 1014 | `				continue;` |
|        - | 1015 | `			}` |
|      ! 0 | 1016 | `			zStr[pos]++;` |
|      ! 0 | 1017 | `			carry = 0;` |
|      ! 0 | 1018 | `			break;` |
|      ! 0 | 1019 | `		}else if( ch >= '0' && ch <= '9' ){` |
|      ! 0 | 1020 | `			if( ch == '9' ){` |
|      ! 0 | 1021 | `				zStr[pos] = '0';` |
|      ! 0 | 1022 | `				last_class = CARRY_DIGIT;` |
|      ! 0 | 1023 | `				continue;` |
|        - | 1024 | `			}` |
|      ! 0 | 1025 | `			zStr[pos]++;` |
|      ! 0 | 1026 | `			carry = 0;` |
|      ! 0 | 1027 | `			break;` |
|      ! 0 | 1028 | `		}else{` |
|        - | 1029 | `			/* non-alphanumeric: stop without prepending */` |
|      ! 0 | 1030 | `			carry = 0;` |
|      ! 0 | 1031 | `			break;` |
|        - | 1032 | `		}` |
|      ! 0 | 1033 | `	}` |
|      ! 0 | 1034 | `	if( carry ){` |
|        - | 1035 | `		sxu8 prepend;` |
|        - | 1036 | `		sxu32 i;` |
|      ! 0 | 1037 | `		switch( last_class ){` |
|      ! 0 | 1038 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|      ! 0 | 1039 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1040 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1041 | `		}` |
|        - | 1042 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|      ! 0 | 1043 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|      ! 0 | 1044 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1045 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1046 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|      ! 0 | 1047 | `		for( i = nLen - 1; i > 0; i-- ){` |
|      ! 0 | 1048 | `			zStr[i] = zStr[i - 1];` |
|      ! 0 | 1049 | `		}` |
|      ! 0 | 1050 | `		zStr[0] = prepend;` |
|      ! 0 | 1051 | `	}` |
|      ! 0 | 1052 | `	return SXRET_OK;` |
|      ! 0 | 1053 | `}` |
|        - | 1054 | `/*` |
|        - | 1055 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1056 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1057 | ` */` |
|     1086 | 1058 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        4 | 1059 | `{` |
|     1090 | 1060 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1061 | `		/* Work only with reals */` |
|     1090 | 1062 | `		MemObjTryIntger(&(*pObj));` |
|      543 | 1063 | `	}` |
|     1090 | 1064 | `	return SXRET_OK;` |
|        4 | 1065 | `}` |
|        - | 1066 | `/*` |
|        - | 1067 | ` * Initialize a ph7_value to the null type.` |
|        - | 1068 | ` */` |
| 30095828 | 1069 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1070 | `{` |
|        - | 1071 | `	/* Zero the structure */` |
| 30095833 | 1072 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1073 | `	/* Initialize fields */` |
| 30095833 | 1074 | `	pObj->pVm = pVm;` |
| 30095833 | 1075 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1076 | `	/* Set the NULL type */` |
| 30095833 | 1077 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 30095833 | 1078 | `	return SXRET_OK;` |
|        5 | 1079 | `}` |
|        - | 1080 | `/*` |
|        - | 1081 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1082 | ` */` |
|  5950338 | 1083 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1084 | `{` |
|        - | 1085 | `	/* Zero the structure */` |
|  5950343 | 1086 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1087 | `	/* Initialize fields */` |
|  5950343 | 1088 | `	pObj->pVm = pVm;` |
|  5950343 | 1089 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1090 | `	/* Set the desired type */` |
|  5950343 | 1091 | `	pObj->x.iVal = iVal;` |
|  5950343 | 1092 | `	pObj->iFlags = MEMOBJ_INT;` |
|  5950343 | 1093 | `	return SXRET_OK;` |
|        5 | 1094 | `}` |
|        - | 1095 | `/*` |
|        - | 1096 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1097 | ` */` |
|    16702 | 1098 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1099 | `{` |
|        - | 1100 | `	/* Zero the structure */` |
|    16707 | 1101 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1102 | `	/* Initialize fields */` |
|    16707 | 1103 | `	pObj->pVm = pVm;` |
|    16707 | 1104 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1105 | `	/* Set the desired type */` |
|    16707 | 1106 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    16707 | 1107 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    16707 | 1108 | `	return SXRET_OK;` |
|        5 | 1109 | `}` |
|        - | 1110 | `/*` |
|        - | 1111 | ` * Initialize a ph7_value to the real type.` |
|        - | 1112 | ` */` |
|       10 | 1113 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        1 | 1114 | `{` |
|        - | 1115 | `	/* Zero the structure */` |
|       11 | 1116 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1117 | `	/* Initialize fields */` |
|       11 | 1118 | `	pObj->pVm = pVm;` |
|       11 | 1119 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1120 | `	/* Set the desired type */` |
|       11 | 1121 | `	pObj->rVal = rVal;` |
|       11 | 1122 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       11 | 1123 | `	return SXRET_OK;` |
|        1 | 1124 | `}` |
|        - | 1125 | `/*` |
|        - | 1126 | ` * Initialize a ph7_value to the array type.` |
|        - | 1127 | ` */` |
|    63552 | 1128 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1129 | `{` |
|        - | 1130 | `	/* Zero the structure */` |
|    63557 | 1131 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1132 | `	/* Initialize fields */` |
|    63557 | 1133 | `	pObj->pVm = pVm;` |
|    63557 | 1134 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1135 | `	/* Set the desired type */` |
|    63557 | 1136 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|    63557 | 1137 | `	pObj->x.pOther = pArray;` |
|    63557 | 1138 | `	return SXRET_OK;` |
|        5 | 1139 | `}` |
|        - | 1140 | `/*` |
|        - | 1141 | ` * Initialize a ph7_value to the string type.` |
|        - | 1142 | ` */` |
|  4885514 | 1143 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1144 | `{` |
|        - | 1145 | `	/* Zero the structure */` |
|  4885519 | 1146 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1147 | `	/* Initialize fields */` |
|  4885519 | 1148 | `	pObj->pVm = pVm;` |
|  4885519 | 1149 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  4885519 | 1150 | `	if( pVal ){` |
|        - | 1151 | `		/* Append contents */` |
|  2466865 | 1152 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|  1233430 | 1153 | `	}` |
|        - | 1154 | `	/* Set the desired type */` |
|  4885519 | 1155 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  4885519 | 1156 | `	return SXRET_OK;` |
|        5 | 1157 | `}` |
|        - | 1158 | `/*` |
|        - | 1159 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1160 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1161 | ` * invalidate any prior representation and set the string type.` |
|        - | 1162 | ` * Then a simple append operation is performed.` |
|        - | 1163 | ` */` |
|  2838229 | 1164 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1165 | `{` |
|        - | 1166 | `	sxi32 rc;` |
|  2838234 | 1167 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1168 | `		/* Invalidate any prior representation */` |
|     3765 | 1169 | `		PH7_MemObjRelease(pObj);` |
|     3765 | 1170 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     1880 | 1171 | `	}` |
|        - | 1172 | `	/* Append contents */` |
|  2838234 | 1173 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  2838234 | 1174 | `	return rc;` |
|        5 | 1175 | `}` |
|        - | 1176 | `#if 0` |
|        - | 1177 | `/*` |
|        - | 1178 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1179 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1180 | ` * any prior representation and set the string type.` |
|        - | 1181 | ` * Then a simple format and append operation is performed.` |
|        - | 1182 | ` */` |
|        - | 1183 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1184 | `{` |
|        - | 1185 | `	sxi32 rc;` |
|        - | 1186 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1187 | `		/* Invalidate any prior representation */` |
|        - | 1188 | `		PH7_MemObjRelease(pObj);` |
|        - | 1189 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1190 | `	}` |
|        - | 1191 | `	/* Format and append contents */` |
|        - | 1192 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1193 | `	return rc;` |
|        - | 1194 | `}` |
|        - | 1195 | `#endif` |
|        - | 1196 | `/*` |
|        - | 1197 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1198 | ` */` |
|  6024521 | 1199 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1200 | `{` |
|  6024526 | 1201 | `	ph7_class_instance *pObj = 0;` |
|  6024526 | 1202 | `	ph7_hashmap *pMap = 0;` |
|        - | 1203 | `	sxi32 rc;` |
|  6024526 | 1204 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1205 | `		/* Increment reference count */` |
|   217935 | 1206 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  5915561 | 1207 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1208 | `		/* Increment reference count */` |
|    12313 | 1209 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     6154 | 1210 | `	}` |
|  6024526 | 1211 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    80833 | 1212 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  5984112 | 1213 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     8781 | 1214 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     4388 | 1215 | `	}` |
|  6024526 | 1216 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  6024526 | 1217 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  6024526 | 1218 | `	rc = SXRET_OK;` |
|  6024526 | 1219 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4255055 | 1220 | `		SyBlobReset(&pDest->sBlob);` |
|  4255055 | 1221 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  2127843 | 1222 | `	}else{` |
|  1769476 | 1223 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   321865 | 1224 | `			SyBlobRelease(&pDest->sBlob);` |
|   161196 | 1225 | `		}` |
|        - | 1226 | `	}` |
|  6024526 | 1227 | `	if( pMap ){` |
|    80833 | 1228 | `		PH7_HashmapUnref(pMap);` |
|  5984112 | 1229 | `	}else if( pObj ){` |
|     8781 | 1230 | `		PH7_ClassInstanceUnref(pObj);` |
|     4388 | 1231 | `	}` |
|  6024521 | 1232 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  3122326 | 1233 | `	 && pDest->pVm` |
|   217930 | 1234 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1235 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1236 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1237 | `	  * for closure envs and other non-slot destinations. */` |
|   108974 | 1238 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1239 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1240 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1241 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1242 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1243 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1244 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1245 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1246 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1247 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1248 | `			pDest->x.pOther = pSnap;` |
|        4 | 1249 | `		}else if( pSnap ){` |
|      ! 0 | 1250 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1251 | `		}` |
|        4 | 1252 | `	}` |
|  6024526 | 1253 | `	return rc;` |
|        5 | 1254 | `}` |
|        - | 1255 | `/*` |
|        - | 1256 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|        - | 1257 | ` * buffer contents,simply point to it.` |
|        - | 1258 | ` */` |
|  8327646 | 1259 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1260 | `{` |
|  8327651 | 1261 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|        - | 1262 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  8327651 | 1263 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1264 | `		/* Increment reference count */` |
|   540161 | 1265 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  8057573 | 1266 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1267 | `		/* Increment reference count */` |
|    56263 | 1268 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    28129 | 1269 | `	}` |
|  8327651 | 1270 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       46 | 1271 | `		SyBlobRelease(&pDest->sBlob);` |
|       21 | 1272 | `	}` |
|  8327651 | 1273 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  4382428 | 1274 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|  2192320 | 1275 | `	}` |
|  8327651 | 1276 | `	return SXRET_OK;` |
|        5 | 1277 | `}` |
|        - | 1278 | `/*` |
|        - | 1279 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1280 | ` */` |
| 20665949 | 1281 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|        5 | 1282 | `{` |
| 20665954 | 1283 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 17685073 | 1284 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   705679 | 1285 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 17332236 | 1286 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   105177 | 1287 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|    52586 | 1288 | `		}` |
|        - | 1289 | `		/* Release the internal buffer */` |
| 17685073 | 1290 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1291 | `		/* Invalidate any prior representation */` |
| 17685073 | 1292 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  8846843 | 1293 | `	}` |
| 20665954 | 1294 | `	return SXRET_OK;` |
|        5 | 1295 | `}` |
|        - | 1296 | `/*` |
|        - | 1297 | ` * Compare two ph7_values.` |
|        - | 1298 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1299 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1300 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1301 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1302 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1303 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1304 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1305 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1306 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1307 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1308 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1309 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1310 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1311 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1312 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1313 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1314 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1315 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1316 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1317 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1318 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1319 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1320 | ` *      Loose comparisons with ==` |
|        - | 1321 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1322 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1323 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1324 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1325 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 1326 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1327 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1328 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1329 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1330 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 1331 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1332 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1333 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 1334 | ` *    Strict comparisons with ===` |
|        - | 1335 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 1336 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1337 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1338 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1339 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1340 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1341 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1342 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1343 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 1344 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 1345 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 1346 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 1347 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 1348 | ` */` |
|  1570966 | 1349 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 1350 | `{` |
|        - | 1351 | `	sxi32 iComb;` |
|        - | 1352 | `	sxi32 rc;` |
|  1570971 | 1353 | `	if( bStrict ){` |
|        - | 1354 | `		sxi32 iF1,iF2;` |
|        - | 1355 | `		/* Strict comparisons with === */` |
|   850204 | 1356 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   850204 | 1357 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   850204 | 1358 | `		if( iF1 != iF2 ){` |
|        - | 1359 | `			/* Not of the same type */` |
|   196485 | 1360 | `			return 1;` |
|        - | 1361 | `		}` |
|   327069 | 1362 | `	}` |
|        - | 1363 | `	/* Combine flag together */` |
|  1374491 | 1364 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  1374486 | 1365 | `	if( !bStrict` |
|  1047836 | 1366 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   360607 | 1367 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       66 | 1368 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 1369 | `		/*` |
|        - | 1370 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 1371 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 1372 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 1373 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 1374 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 1375 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 1376 | `		 */` |
|       45 | 1377 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       35 | 1378 | `			PH7_MemObjToString(pObj1);` |
|       18 | 1379 | `		}else{` |
|       11 | 1380 | `			PH7_MemObjToString(pObj2);` |
|        - | 1381 | `		}` |
|       45 | 1382 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 1383 | `	}` |
|  1374491 | 1384 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|        - | 1385 | `		/* php compares two resources by their ID. The boolean path below would` |
|        - | 1386 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        5 | 1387 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        5 | 1388 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        5 | 1389 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|        - | 1390 | `	}` |
|  1374487 | 1391 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|        - | 1392 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|    38297 | 1393 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    23327 | 1394 | `			PH7_MemObjToBool(pObj1);` |
|    11661 | 1395 | `		}` |
|    38297 | 1396 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    22273 | 1397 | `			PH7_MemObjToBool(pObj2);` |
|    11134 | 1398 | `		}` |
|    38297 | 1399 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  1336195 | 1400 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 1401 | `		/* Hashmap aka 'array' comparison */` |
|       58 | 1402 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1403 | `			/* Array is always greater */` |
|      ! 0 | 1404 | `			return -1;` |
|        - | 1405 | `		}` |
|       58 | 1406 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1407 | `			/* Array is always greater */` |
|      ! 0 | 1408 | `			return 1;` |
|        - | 1409 | `		}` |
|        - | 1410 | `		/* Perform the comparison */` |
|       58 | 1411 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       58 | 1412 | `		return rc;` |
|  1336139 | 1413 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 1414 | `		/* Object comparison */` |
|      295 | 1415 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1416 | `			/* Object is always greater */` |
|      ! 0 | 1417 | `			return -1;` |
|        - | 1418 | `		}` |
|      295 | 1419 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1420 | `			/* Object is always greater */` |
|      ! 0 | 1421 | `			return 1;` |
|        - | 1422 | `		}` |
|        - | 1423 | `		/* Perform the comparison */` |
|      295 | 1424 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      295 | 1425 | `		return rc;` |
|  1335849 | 1426 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 1427 | `		SyString s1,s2;` |
|   857911 | 1428 | `		if( !bStrict ){` |
|        - | 1429 | `			/*` |
|        - | 1430 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 1431 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 1432 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 1433 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 1434 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 1435 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 1436 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 1437 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 1438 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 1439 | `			 * below, unchanged.` |
|        - | 1440 | `			 */` |
|   245393 | 1441 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 1442 | `				/* Perform a numeric comparison */` |
|     1103 | 1443 | `				goto Numeric;` |
|        - | 1444 | `			}` |
|   122014 | 1445 | `		}` |
|        - | 1446 | `		/* Perform a strict string comparison.*/` |
|   856809 | 1447 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       23 | 1448 | `			PH7_MemObjToString(pObj1);` |
|       11 | 1449 | `		}` |
|   856809 | 1450 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        7 | 1451 | `			PH7_MemObjToString(pObj2);` |
|        3 | 1452 | `		}` |
|   856809 | 1453 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   856809 | 1454 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 1455 | `		/*` |
|        - | 1456 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 1457 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 1458 | `		 */` |
|   856809 | 1459 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   856809 | 1460 | `		if( rc == 0 ){` |
|   280954 | 1461 | `			if( s1.nByte != s2.nByte ){` |
|    18874 | 1462 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     9436 | 1463 | `			}` |
|   140476 | 1464 | `		}` |
|   856809 | 1465 | `		return rc;` |
|   477943 | 1466 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   238655 | 1467 | `Numeric:` |
|        - | 1468 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   479045 | 1469 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1083 | 1470 | `			PH7_MemObjToNumeric(pObj1);` |
|      541 | 1471 | `		}` |
|   479045 | 1472 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|     1089 | 1473 | `			PH7_MemObjToNumeric(pObj2);` |
|      544 | 1474 | `		}` |
|   479045 | 1475 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 1476 | `			/*` |
|        - | 1477 | `			 * Symisc eXtension to the PHP language:` |
|        - | 1478 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 1479 | `			 */` |
|        - | 1480 | `			ph7_real r1,r2;` |
|        - | 1481 | `			/* Compare as reals */` |
|      310 | 1482 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       11 | 1483 | `				PH7_MemObjToReal(pObj1);` |
|        5 | 1484 | `			}` |
|      310 | 1485 | `			r1 = pObj1->rVal;` |
|      310 | 1486 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       56 | 1487 | `				PH7_MemObjToReal(pObj2);` |
|       27 | 1488 | `			}` |
|      310 | 1489 | `			r2 = pObj2->rVal;` |
|      310 | 1490 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 1491 | `				/*` |
|        - | 1492 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|        - | 1493 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|        - | 1494 | `				 * any non-NaN numeric value.` |
|        - | 1495 | `				 */` |
|       50 | 1496 | `				if( PH7_IS_NAN(r1) ){` |
|       40 | 1497 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|        - | 1498 | `				}` |
|       11 | 1499 | `				return -1;` |
|        - | 1500 | `			}` |
|      262 | 1501 | `			if( r1 > r2 ){` |
|       54 | 1502 | `				return 1;` |
|      210 | 1503 | `			}else if( r1 < r2 ){` |
|      134 | 1504 | `				return -1;` |
|        - | 1505 | `			}` |
|       78 | 1506 | `			return 0;` |
|      ! 0 | 1507 | `		}else{` |
|        - | 1508 | `			/* Integer comparison */` |
|   478737 | 1509 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|     7685 | 1510 | `				return 1;` |
|   471057 | 1511 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   459987 | 1512 | `				return -1;` |
|        - | 1513 | `			}` |
|    11075 | 1514 | `			return 0;` |
|        - | 1515 | `		}` |
|        - | 1516 | `	}` |
|        - | 1517 | `	/* NOT REACHED */` |
|      ! 0 | 1518 | `	return 0;` |
|   785882 | 1519 | `}` |
|        - | 1520 | `/*` |
|        - | 1521 | ` * Perform an addition operation of two ph7_values.` |
|        - | 1522 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 1523 | ` * is that the '+' operator is overloaded.` |
|        - | 1524 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 1525 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 1526 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 1527 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 1528 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 1529 | ` * be ignored.` |
|        - | 1530 | ` * This function take care of handling all the scenarios.` |
|        - | 1531 | ` */` |
|    14626 | 1532 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 1533 | `{` |
|    14631 | 1534 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1535 | `			/* Arithemtic operation */` |
|    10829 | 1536 | `			PH7_MemObjToNumeric(pObj1);` |
|    10829 | 1537 | `			PH7_MemObjToNumeric(pObj2);` |
|    10829 | 1538 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 1539 | `				/* Floating point arithmetic */` |
|        - | 1540 | `				ph7_real a,b;` |
|       69 | 1541 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       29 | 1542 | `					PH7_MemObjToReal(pObj1);` |
|       14 | 1543 | `				}` |
|       69 | 1544 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 | 1545 | `					PH7_MemObjToReal(pObj2);` |
|        4 | 1546 | `				}` |
|       69 | 1547 | `				a = pObj1->rVal;` |
|       69 | 1548 | `				b = pObj2->rVal;` |
|       69 | 1549 | `				pObj1->rVal = a+b;` |
|       69 | 1550 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1551 | `				/* Try to get an integer representation also */` |
|       69 | 1552 | `				MemObjTryIntger(&(*pObj1));` |
|       35 | 1553 | `			}else{` |
|        - | 1554 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 1555 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 1556 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 1557 | `				sxi64 a,b,r;` |
|    10761 | 1558 | `				a = pObj1->x.iVal;` |
|    10761 | 1559 | `				b = pObj2->x.iVal;` |
|    10761 | 1560 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 1561 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        9 | 1562 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        9 | 1563 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 1564 | `#else` |
|        - | 1565 | `					pObj1->x.iVal = r;` |
|        - | 1566 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1567 | `#endif` |
|        5 | 1568 | `				}else{` |
|    10753 | 1569 | `					pObj1->x.iVal = r;` |
|    10753 | 1570 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 1571 | `				}` |
|        - | 1572 | `			}` |
|     5417 | 1573 | `	}else{` |
|     3807 | 1574 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 1575 | `			ph7_hashmap *pMap;` |
|        - | 1576 | `			sxi32 rc;` |
|     3807 | 1577 | `			if( bAddStore ){` |
|        - | 1578 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 1579 | `				 */` |
|        3 | 1580 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1581 | `					/* Force a hashmap cast */` |
|      ! 0 | 1582 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 1583 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 1584 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1585 | `						return rc;` |
|        - | 1586 | `					}` |
|      ! 0 | 1587 | `				}` |
|        - | 1588 | `				/* COW separate before in-place mutation */` |
|        3 | 1589 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        2 | 1590 | `			}else{` |
|        - | 1591 | `				/* Create a new hashmap */` |
|     3805 | 1592 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     3805 | 1593 | `				if( pMap == 0){` |
|      ! 0 | 1594 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 1595 | `					return SXERR_MEM;` |
|        - | 1596 | `				}` |
|        - | 1597 | `			}` |
|     3807 | 1598 | `			if( !bAddStore ){` |
|     3805 | 1599 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1600 | `					/* Perform a hashmap duplication */` |
|     3805 | 1601 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     1905 | 1602 | `				}else{` |
|      ! 0 | 1603 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1604 | `						/* Simple insertion */` |
|      ! 0 | 1605 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 1606 | `					}` |
|        - | 1607 | `				}` |
|     1900 | 1608 | `			}` |
|        - | 1609 | `			/* Perform the union */` |
|     3807 | 1610 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     3807 | 1611 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     1906 | 1612 | `			}else{` |
|      ! 0 | 1613 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1614 | `					/* Simple insertion */` |
|      ! 0 | 1615 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 1616 | `				}` |
|        - | 1617 | `			}` |
|        - | 1618 | `			/* Reflect the change */` |
|     3807 | 1619 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1620 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 1621 | `			}` |
|     3807 | 1622 | `			pObj1->x.pOther = pMap;` |
|     3807 | 1623 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     1901 | 1624 | `		}` |
|        - | 1625 | `	}` |
|    14631 | 1626 | `	return SXRET_OK;` |
|     7318 | 1627 | `}` |
|        - | 1628 | `/*` |
|        - | 1629 | ` * Return a printable representation of the type of a given` |
|        - | 1630 | ` * ph7_value.` |
|        - | 1631 | ` */` |
|      ! 0 | 1632 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|      ! 0 | 1633 | `{` |
|      ! 0 | 1634 | `	const char *zType = "";` |
|      ! 0 | 1635 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 1636 | `		zType = "null";` |
|      ! 0 | 1637 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 1638 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 1639 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 1640 | `		zType = "double";` |
|      ! 0 | 1641 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 1642 | `		zType = "int";` |
|      ! 0 | 1643 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 1644 | `		zType = "string";` |
|      ! 0 | 1645 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1646 | `		zType = "bool";` |
|      ! 0 | 1647 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      ! 0 | 1648 | `		zType = "array";` |
|      ! 0 | 1649 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1650 | `		zType = "object";` |
|      ! 0 | 1651 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 1652 | `		zType = "resource";` |
|      ! 0 | 1653 | `	}` |
|      ! 0 | 1654 | `	return zType;` |
|      ! 0 | 1655 | `}` |
|        - | 1656 | `/*` |
|        - | 1657 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 1658 | ` * Store the dump in the given blob.` |
|        - | 1659 | ` */` |
|        - | 1660 | `/*` |
|        - | 1661 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 1662 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 1663 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 1664 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 1665 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 1666 | ` */` |
|       48 | 1667 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        1 | 1668 | `{` |
|        - | 1669 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 1670 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|        - | 1671 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|        - | 1672 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|        - | 1673 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|        - | 1674 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|        - | 1675 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|        - | 1676 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|        - | 1677 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       49 | 1678 | `	PH7_AppendShortestReal(pOut,rVal);` |
|        - | 1679 | `#else` |
|        - | 1680 | `	if( PH7_IS_NAN(rVal) ){` |
|        - | 1681 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|        - | 1682 | `	}else if( PH7_IS_INF(rVal) ){` |
|        - | 1683 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|        - | 1684 | `	}else{` |
|        - | 1685 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 1686 | `	}` |
|        - | 1687 | `#endif` |
|       49 | 1688 | `}` |
|        - | 1689 | `/*` |
|        - | 1690 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 1691 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 1692 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 1693 | ` */` |
|      216 | 1694 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        2 | 1695 | `{` |
|      218 | 1696 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        7 | 1697 | `		return;` |
|        - | 1698 | `	}` |
|      212 | 1699 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 1700 | `		if( pObj->x.iVal != 0 ){` |
|      ! 0 | 1701 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|      ! 0 | 1702 | `		}` |
|      ! 0 | 1703 | `		return;` |
|        - | 1704 | `	}` |
|      212 | 1705 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1706 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 1707 | `		 * non-strings into the output) */` |
|      112 | 1708 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      112 | 1709 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       55 | 1710 | `		}` |
|      112 | 1711 | `		return;` |
|        - | 1712 | `	}` |
|      102 | 1713 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      110 | 1714 | `}` |
|     1142 | 1715 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 1716 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 1717 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 1718 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 1719 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 1720 | `	                    * print_r = the container's parenthesis column */` |
|        - | 1721 | `	int nDepth,        /* Nesting level */` |
|        - | 1722 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 1723 | `	)` |
|        5 | 1724 | `{` |
|     1147 | 1725 | `	sxi32 rc = SXRET_OK;` |
|        - | 1726 | `	int i;` |
|     1147 | 1727 | `	if( !ShowType ){` |
|        - | 1728 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 1729 | `		 * containers render the Array/Object block (which the container` |
|        - | 1730 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      114 | 1731 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      107 | 1732 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1733 | `		}` |
|        8 | 1734 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        6 | 1735 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 1736 | `		}` |
|        3 | 1737 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        3 | 1738 | `		return SXRET_OK;` |
|        - | 1739 | `	}` |
|        - | 1740 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 1741 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 1742 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     5155 | 1743 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     4123 | 1744 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     2063 | 1745 | `	}` |
|     1035 | 1746 | `	if( isRef ){` |
|        7 | 1747 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        3 | 1748 | `	}` |
|     1035 | 1749 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      139 | 1750 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      139 | 1751 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 1752 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|      ! 0 | 1753 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|      ! 0 | 1754 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|      ! 0 | 1755 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      ! 0 | 1756 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|      ! 0 | 1757 | `			}` |
|      ! 0 | 1758 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      ! 0 | 1759 | `			return SXRET_OK;` |
|        - | 1760 | `		}` |
|      139 | 1761 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      139 | 1762 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      139 | 1763 | `		return rc;` |
|        - | 1764 | `	}` |
|      899 | 1765 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       23 | 1766 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       23 | 1767 | `		return SXRET_OK;` |
|        - | 1768 | `	}` |
|      877 | 1769 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       30 | 1770 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       30 | 1771 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       30 | 1772 | `		return rc;` |
|        - | 1773 | `	}` |
|      849 | 1774 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      334 | 1775 | `		if( pObj->x.iVal != 0 ){` |
|      224 | 1776 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|      114 | 1777 | `		}else{` |
|      114 | 1778 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 1779 | `		}` |
|      334 | 1780 | `		return SXRET_OK;` |
|        - | 1781 | `	}` |
|      518 | 1782 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1783 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 1784 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       49 | 1785 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       49 | 1786 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       49 | 1787 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       49 | 1788 | `		return SXRET_OK;` |
|        - | 1789 | `	}` |
|      470 | 1790 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      300 | 1791 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      300 | 1792 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      300 | 1793 | `		return SXRET_OK;` |
|        - | 1794 | `	}` |
|      173 | 1795 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      173 | 1796 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      173 | 1797 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      153 | 1798 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       75 | 1799 | `		}` |
|      173 | 1800 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      173 | 1801 | `		return SXRET_OK;` |
|        - | 1802 | `	}` |
|      ! 0 | 1803 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|        - | 1804 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|        - | 1805 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|        - | 1806 | `		 * shape printed the heap pointer through the string cast instead. */` |
|      ! 0 | 1807 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|      ! 0 | 1808 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|      ! 0 | 1809 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|      ! 0 | 1810 | `		return SXRET_OK;` |
|        - | 1811 | `	}` |
|        - | 1812 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|        - | 1813 | `	{` |
|      ! 0 | 1814 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 1815 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 1816 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 1817 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 1818 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 1819 | `	}` |
|      ! 0 | 1820 | `	return rc;` |
|      576 | 1821 | `}` |
|        - | 1822 |  |
