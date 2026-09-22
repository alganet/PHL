# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 939/1121 lines (83.76%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h" /* This file handle low-level stuff related to indexed memory objects [i.e: ph7_value] */` |
|         - |    7 | `#include <stdio.h>  /* snprintf — the default float->string conversion needs` |
|         - |    8 | `                     * correctly-rounded digits like php (see MemObjStringValue) */` |
|         - |    9 | `#include <stdlib.h> /* strtod — var_dump's shortest-round-trip float shape` |
|         - |   10 | `                     * verifies each candidate by parsing it back */` |
|         - |   11 |  |
|         - |   12 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|         - |   13 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|         - |   14 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|         - |   15 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|         - |   16 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|         - |   17 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|         - |   18 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|         - |   19 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|         - |   20 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|         - |   21 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   22 | `{` |
|         5 |   23 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|         - |   24 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|         5 |   25 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|         5 |   26 | `}` |
|         - |   27 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   28 | `{` |
|         5 |   29 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|         - |   30 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|         - |   31 | `	 * from the minuend's. */` |
|         5 |   32 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|         5 |   33 | `}` |
|         - |   34 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |   35 | `{` |
|         5 |   36 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|         5 |   37 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|         4 |   38 | `		return 0;` |
|         - |   39 | `	}` |
|         5 |   40 | `	if( a == -1 ){` |
|         1 |   41 | `		return b == SMALLEST_INT64;` |
|         - |   42 | `	}` |
|         5 |   43 | `	if( b == -1 ){` |
|         1 |   44 | `		return a == SMALLEST_INT64;` |
|         - |   45 | `	}` |
|         5 |   46 | `	if( a > 0 ){` |
|         5 |   47 | `		if( b > 0 ){` |
|         5 |   48 | `			return a > LARGEST_INT64 / b;` |
|       ! 0 |   49 | `		}else{` |
|         1 |   50 | `			return b < SMALLEST_INT64 / a;` |
|         - |   51 | `		}` |
|       ! 0 |   52 | `	}else{` |
|         1 |   53 | `		if( b > 0 ){` |
|         1 |   54 | `			return a < SMALLEST_INT64 / b;` |
|       ! 0 |   55 | `		}else{` |
|         1 |   56 | `			return b < LARGEST_INT64 / a;` |
|         - |   57 | `		}` |
|         - |   58 | `	}` |
|         5 |   59 | `}` |
|         - |   60 | `#endif` |
|         - |   61 |  |
|         - |   62 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|         - |   63 | ` * by any subsystem that works with ph7_value.` |
|         - |   64 | ` */` |
|       876 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|       881 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|       825 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|       815 |   69 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|       565 |   70 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|       541 |   71 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       171 |   72 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        28 |   73 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        25 |   74 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   75 | `	return "unknown";` |
|       443 |   76 | `}` |
|         - |   77 |  |
|         - |   78 | `/*` |
|         - |   79 | ` * Notes on memory objects [i.e: ph7_value].` |
|         - |   80 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |   81 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|         - |   82 | ` * Each ph7_values struct may cache multiple representations (string,` |
|         - |   83 | ` * integer etc.) of the same value.` |
|         - |   84 | ` */` |
|         - |   85 | `/*` |
|         - |   86 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|         - |   87 | ` * If the double is too large, return 0x8000000000000000.` |
|         - |   88 | ` *` |
|         - |   89 | ` * Most systems appear to do this simply by assigning ariables and without` |
|         - |   90 | ` * the extra range tests.` |
|         - |   91 | ` * But there are reports that windows throws an expection if the floating` |
|         - |   92 | ` * point value is out of range.` |
|         - |   93 | ` */` |
|     13570 |   94 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |   95 | `{` |
|         - |   96 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |   97 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |   98 | `	 * is omitted from the build.` |
|         - |   99 | `	 */` |
|         - |  100 | `	return pObj->rVal;` |
|         - |  101 | `#else` |
|         - |  102 | ` /*` |
|         - |  103 | `  ** Many compilers we encounter do not define constants for the` |
|         - |  104 | `  ** minimum and maximum 64-bit integers, or they define them` |
|         - |  105 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|         - |  106 | `  ** So we define our own static constants here using nothing` |
|         - |  107 | `  ** larger than a 32-bit integer constant.` |
|         - |  108 | `  */` |
|         - |  109 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     13575 |  110 | `  ph7_real r = pObj->rVal;` |
|         - |  111 | `  /* The bounds are tested in DOUBLE space, and the arithmetic there is exact:` |
|         - |  112 | `  ** (ph7_real)minInt is -2^63 to the bit, so -(ph7_real)minInt is +2^63 -- one` |
|         - |  113 | `  ** past the range -- and no double exists between LARGEST_INT64 and it. Hence` |
|         - |  114 | ``  ** `>=` on the way up and `<` on the way down.`` |
|         - |  115 | `  **` |
|         - |  116 | ``  ** The upper test used to be `r > (ph7_real)maxInt`, and (ph7_real)maxInt ROUNDS`` |
|         - |  117 | ``  ** UP to 2^63: a double of exactly 2^63 passed the guard and reached `(sxi64)r`,`` |
|         - |  118 | `  ** which is undefined behaviour. x86 happens to answer minInt there -- the same` |
|         - |  119 | `  ** value this returns -- but aarch64 saturates to maxInt, so PHL answered two` |
|         - |  120 | ``  ** different values for `(int)9.2233720368547758E+18` on the two platforms it`` |
|         - |  121 | `  ** builds for. NaN compares false against every bound and reached the same cast,` |
|         - |  122 | `  ** so it is screened here too.` |
|         - |  123 | `  **` |
|         - |  124 | `  ** minInt is deliberate for BOTH directions, not maxInt going up: it is the` |
|         - |  125 | `  ** answer x86's cast produced, and the corpus pins it. php's own answer for a` |
|         - |  126 | `  ** non-representable float is a modular wrap (and a warning) -- a divergence` |
|         - |  127 | `  ** recorded in §2, not something this boundary fix changes. */` |
|     13575 |  128 | `  if( PH7_IS_NAN(r) \|\| r < (ph7_real)minInt \|\| r >= -(ph7_real)minInt ){` |
|       784 |  129 | `    return minInt;` |
|       ! 0 |  130 | `  }else{` |
|     12795 |  131 | `    return (sxi64)r;` |
|         - |  132 | `  }` |
|         - |  133 | `#endif` |
|      6790 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  137 | ` * to a 64-bit integer.` |
|         - |  138 | ` */` |
|   4481436 |  139 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  140 | `{` |
|   4481441 |  141 | `	sxi64 iVal = 0;` |
|   4481441 |  142 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  143 | `		return 0;` |
|         - |  144 | `	}` |
|   4481441 |  145 | `	if( pVal->zString[0] == '0' ){` |
|         - |  146 | `		sxi32 c;` |
|   1778341 |  147 | `		if( pVal->nByte == sizeof(char) ){` |
|   1651085 |  148 | `			return 0;` |
|         - |  149 | `		}` |
|    127261 |  150 | `		c = pVal->zString[1];` |
|    127261 |  151 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  152 | `			/* Hex digit stream */` |
|    122355 |  153 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     66086 |  154 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  155 | `			/* Binary digit stream */` |
|       285 |  156 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      4769 |  157 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  158 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  159 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  160 | `			if( pVal->nByte > 2 ){` |
|        21 |  161 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  162 | `			}` |
|        11 |  163 | `		}else{` |
|         - |  164 | `			/* Legacy octal digit stream (leading 0) */` |
|      4607 |  165 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  166 | `		}` |
|     63633 |  167 | `	}else{` |
|         - |  168 | `		/* Decimal digit stream */` |
|   2703105 |  169 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  170 | `	}` |
|   2830361 |  171 | `	return iVal;` |
|   2240723 |  172 | `}` |
|         - |  173 | `/*` |
|         - |  174 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  175 | ` * do at representing the value that pObj describes as a string` |
|         - |  176 | ` * representation.` |
|         - |  177 | ` */` |
|      6806 |  178 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  179 | `{` |
|      6811 |  180 | `	sxi64 iVal = 0;` |
|         - |  181 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  182 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  183 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      6811 |  184 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|      6811 |  185 | `	return iVal;` |
|         5 |  186 | `}` |
|         - |  187 | `/*` |
|         - |  188 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  189 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  190 | ` * successfully called. Any other return value indicates failure.` |
|         - |  191 | ` */` |
|       510 |  192 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  193 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  194 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  195 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  196 | `	sxu32 nLen,                /* Method name length */` |
|         - |  197 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  198 | `	)` |
|         5 |  199 | `{` |
|         - |  200 | `	ph7_class_method *pMethod;` |
|         - |  201 | `	/* Check if the method is available */` |
|       515 |  202 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       515 |  203 | `	if( pMethod == 0 ){` |
|         - |  204 | `		/* No such method */` |
|         6 |  205 | `		return SXERR_NOTFOUND;` |
|         - |  206 | `	}` |
|         - |  207 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  208 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  209 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  210 | `	 * printed "Object" after a caught __toString() throw). */` |
|       511 |  211 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       260 |  212 | `}` |
|         - |  213 | `/*` |
|         - |  214 | ` * Return some kind of integer value which is the best we can` |
|         - |  215 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  216 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  217 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  218 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  219 | ` * a integer and return that.` |
|         - |  220 | ` * If pObj represents a NULL value, return 0.` |
|         - |  221 | ` */` |
|      1850 |  222 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  223 | `{` |
|         - |  224 | `	sxi32 iFlags;` |
|      1855 |  225 | `	iFlags = pObj->iFlags;` |
|      1855 |  226 | `	if (iFlags & MEMOBJ_REAL ){` |
|        75 |  227 | `		return MemObjRealToInt(&(*pObj));` |
|      1785 |  228 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       220 |  229 | `		return pObj->x.iVal;` |
|      1569 |  230 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  231 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  232 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  233 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|      1521 |  234 | `		return MemObjStringToInt(&(*pObj),0);` |
|        50 |  235 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        30 |  236 | `		return 0;` |
|        22 |  237 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  238 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  239 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  240 | `		 * followed php; int/float did not.) */` |
|         7 |  241 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|         7 |  242 | `		sxu32 n = pMap->nEntry;` |
|         7 |  243 | `		PH7_HashmapUnref(pMap);` |
|         7 |  244 | `		return n > 0 ? 1 : 0;` |
|        16 |  245 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  246 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  247 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  248 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|         7 |  249 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         7 |  250 | `		if( pInst && pInst->pClass ){` |
|        10 |  251 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         6 |  252 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|         3 |  253 | `		}` |
|         7 |  254 | `		PH7_ClassInstanceUnref(pInst);` |
|         7 |  255 | `		return 1;` |
|        10 |  256 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  257 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  258 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        10 |  259 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  260 | `	}` |
|         - |  261 | `	/* CANT HAPPEN */` |
|       ! 0 |  262 | `	return 0;` |
|       930 |  263 | `}` |
|         - |  264 | `/*` |
|         - |  265 | ` * Return some kind of real value which is the best we can` |
|         - |  266 | ` * do at representing the value that pObj describes as a real.` |
|         - |  267 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  268 | ` * integer then the integer  is promoted to real and that value` |
|         - |  269 | ` * is returned.` |
|         - |  270 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  271 | ` * into a real and return that.` |
|         - |  272 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  273 | ` */` |
|     12148 |  274 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  275 | `{` |
|         - |  276 | `	sxi32 iFlags;` |
|     12153 |  277 | `	iFlags = pObj->iFlags;` |
|     12153 |  278 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  279 | `		return pObj->rVal;` |
|     12153 |  280 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      1173 |  281 | `		return (ph7_real)pObj->x.iVal;` |
|     10985 |  282 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  283 | `		SyString sString;` |
|         - |  284 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  285 | `		ph7_real rVal = 0;` |
|         - |  286 | `#else` |
|     10965 |  287 | `		ph7_real rVal = 0.0;` |
|         - |  288 | `#endif` |
|     10965 |  289 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     10965 |  290 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  291 | `			/* Convert as much as we can */` |
|         - |  292 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  293 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  294 | `#else` |
|     10961 |  295 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  296 | `#endif` |
|      5478 |  297 | `		}` |
|     10965 |  298 | `		return rVal;` |
|        22 |  299 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  300 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  301 | `		return 0;` |
|         - |  302 | `#else` |
|         9 |  303 | `		return 0.0;` |
|         - |  304 | `#endif` |
|        14 |  305 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  306 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  307 | `		 * branch above. */` |
|       ! 0 |  308 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  309 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  310 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  311 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        14 |  312 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  313 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        12 |  314 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        12 |  315 | `		if( pInst && pInst->pClass ){` |
|        17 |  316 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        10 |  317 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|         5 |  318 | `		}` |
|        12 |  319 | `		PH7_ClassInstanceUnref(pInst);` |
|        12 |  320 | `		return (ph7_real)1.0;` |
|         3 |  321 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  322 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  323 | `	}` |
|         - |  324 | `	/* NOT REACHED  */` |
|       ! 0 |  325 | `	return 0;` |
|      6079 |  326 | `}` |
|         - |  327 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  328 | `/*` |
|         - |  329 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  330 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  331 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  332 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  333 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  334 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  335 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  336 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  337 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  338 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  339 | ` */` |
|       626 |  340 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  341 | `{` |
|         - |  342 | `	sxi32 iExp,i;` |
|       631 |  343 | `	iExp = nLen - 1;` |
|      4847 |  344 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      4221 |  345 | `		iExp--;` |
|         5 |  346 | `	}` |
|       631 |  347 | `	if( iExp <= 0 ){` |
|       581 |  348 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  349 | `	}` |
|         - |  350 | `	{` |
|        52 |  351 | `		sxi32 iDig = iExp + 1;` |
|         - |  352 | `		sxi32 iFirst;` |
|        52 |  353 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|        52 |  354 | `			iDig++;` |
|        25 |  355 | `		}` |
|        52 |  356 | `		iFirst = iDig;` |
|        87 |  357 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|        64 |  358 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        25 |  359 | `			iFirst++;` |
|         1 |  360 | `		}` |
|        52 |  361 | `		if( iFirst > iDig ){` |
|        25 |  362 | `			sxi32 nStrip = iFirst - iDig;` |
|        73 |  363 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        49 |  364 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        25 |  365 | `			}` |
|        25 |  366 | `			nLen -= nStrip;` |
|        12 |  367 | `		}` |
|         - |  368 | `	}` |
|        52 |  369 | `	if( bGeneric ){` |
|        36 |  370 | `		int bHasDot = 0;` |
|        72 |  371 | `		for( i = 0 ; i < iExp ; i++ ){` |
|        50 |  372 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        20 |  373 | `		}` |
|        36 |  374 | `		if( !bHasDot ){` |
|       132 |  375 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       110 |  376 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        56 |  377 | `			}` |
|        24 |  378 | `			zBuf[iExp] = '.';` |
|        24 |  379 | `			zBuf[iExp+1] = '0';` |
|        24 |  380 | `			nLen += 2;` |
|        11 |  381 | `		}` |
|        17 |  382 | `	}` |
|        52 |  383 | `	return nLen;` |
|       318 |  384 | `}` |
|         - |  385 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  386 | `/*` |
|         - |  387 | ` * Return the string representation of a given ph7_value.` |
|         - |  388 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  389 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  390 | ` * left without a rendering of pObj.` |
|         - |  391 | ` */` |
|     65078 |  392 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  393 | `{` |
|     65083 |  394 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  395 | `		/* Handle special floating-point values first */` |
|       431 |  396 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|       ! 0 |  397 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       431 |  398 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|         5 |  399 | `			if( pObj->rVal < 0.0 ){` |
|       ! 0 |  400 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|       ! 0 |  401 | `			}else{` |
|         5 |  402 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  403 | `			}` |
|         3 |  404 | `		}else{` |
|         - |  405 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  406 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  407 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  408 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  409 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  410 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  411 | `			 * exponent/fraction quirks. */` |
|         - |  412 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       427 |  413 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       427 |  414 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  415 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  416 | `			}` |
|       427 |  417 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       427 |  418 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  419 | `#else` |
|         - |  420 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  421 | `#endif` |
|         5 |  422 | `		}` |
|     64870 |  423 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     63789 |  424 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  425 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     32765 |  426 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       111 |  427 | `		if( bStrictBool ){` |
|         - |  428 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       111 |  429 | `			if( pObj->x.iVal ){` |
|        73 |  430 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        34 |  431 | `			}` |
|         - |  432 | `			/* false produces empty string, nothing to append */` |
|        58 |  433 | `		}else{` |
|         - |  434 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  435 | `			if( pObj->x.iVal ){` |
|       ! 0 |  436 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  437 | `			}else{` |
|       ! 0 |  438 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  439 | `			}` |
|         5 |  440 | `		}` |
|       820 |  441 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       115 |  442 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       115 |  443 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|       711 |  444 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  445 | `		ph7_value sResult;` |
|         - |  446 | `		sxi32 rc;` |
|         - |  447 | `		/* Invoke the __toString() method if available */` |
|       515 |  448 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       515 |  449 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  450 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       515 |  451 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  452 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  453 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  454 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  455 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  456 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  457 | `			 * instance reference. */` |
|       145 |  458 | `			PH7_MemObjRelease(&sResult);` |
|       145 |  459 | `			return rc;` |
|         - |  460 | `		}` |
|       375 |  461 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  462 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  463 | `			 * value, and requiring a non-empty one sent` |
|         - |  464 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  465 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  466 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  467 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  468 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  469 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  470 | `			 * result no return-type check governs. */` |
|       371 |  471 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       188 |  472 | `		}else{` |
|         - |  473 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  474 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         6 |  475 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  476 | `		}` |
|       375 |  477 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       375 |  478 | `		PH7_MemObjRelease(&sResult);` |
|       330 |  479 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  480 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  481 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  482 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  483 | `	}` |
|     64943 |  484 | `	return SXRET_OK;` |
|     32544 |  485 | `}` |
|         - |  486 | `/*` |
|         - |  487 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  488 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  489 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  490 | ` * (php's exact set):` |
|         - |  491 | ` * NULL` |
|         - |  492 | ` * the boolean FALSE itself.` |
|         - |  493 | ` * the integer 0 (zero).` |
|         - |  494 | ` * the real 0.0 (zero).` |
|         - |  495 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  496 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  497 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  498 | ` * and were removed under the §10 PH7-ism policy).` |
|         - |  499 | ` * an array with zero elements.` |
|         - |  500 | ` */` |
|     61992 |  501 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  502 | `{` |
|         - |  503 | `	sxi32 iFlags;` |
|     61997 |  504 | `	iFlags = pObj->iFlags;` |
|     61997 |  505 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  506 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  507 | `		return pObj->rVal ? 1 : 0;` |
|         - |  508 | `#else` |
|        17 |  509 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  510 | `#endif` |
|     61983 |  511 | `	}else if( iFlags & MEMOBJ_INT ){` |
|      1023 |  512 | `		return pObj->x.iVal ? 1 : 0;` |
|     60965 |  513 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  514 | `		SyString sString;` |
|        97 |  515 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  516 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|        97 |  517 | `		if( sString.nByte == 0 ){` |
|        19 |  518 | `			return 0;` |
|         - |  519 | `		}` |
|        81 |  520 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        10 |  521 | `			return 0;` |
|         - |  522 | `		}` |
|        73 |  523 | `		return 1;` |
|     60871 |  524 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     59491 |  525 | `		return 0;` |
|      1385 |  526 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        20 |  527 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        20 |  528 | `		sxu32 n = pMap->nEntry;` |
|        20 |  529 | `		PH7_HashmapUnref(pMap);` |
|        20 |  530 | `		return n > 0 ? TRUE : FALSE;` |
|      1367 |  531 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  532 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  533 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  534 | `		 * extension changed control flow in valid php source. */` |
|       196 |  535 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       196 |  536 | `		return 1;` |
|      1173 |  537 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1173 |  538 | `		return pObj->x.pOther != 0;` |
|         - |  539 | `	}` |
|         - |  540 | `	/* NOT REACHED */` |
|       ! 0 |  541 | `	return 0;` |
|     31001 |  542 | `}` |
|         - |  543 | `/*` |
|         - |  544 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  545 | ` */` |
|     13500 |  546 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  547 | `{` |
|     13505 |  548 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  549 | `  /* Only mark the value as an integer if` |
|         - |  550 | `  **` |
|         - |  551 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  552 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  553 | `  **        possible integer` |
|         - |  554 | `  **` |
|         - |  555 | `  ** The second and third terms in the following conditional enforces` |
|         - |  556 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  557 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  558 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  559 | `  ** architectures might behave differently.` |
|         - |  560 | `  */` |
|     13500 |  561 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     11331 |  562 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     11307 |  563 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      5653 |  564 | `	}` |
|     13505 |  565 | `	return SXRET_OK;` |
|         5 |  566 | `}` |
|         - |  567 | `/*` |
|         - |  568 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  569 | ` */` |
|    661321 |  570 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  571 | `{` |
|    661326 |  572 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  573 | `		/* Preform the conversion */` |
|      1855 |  574 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  575 | `		/* Invalidate any prior representations */` |
|      1855 |  576 | `		SyBlobRelease(&pObj->sBlob);` |
|      1855 |  577 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|       925 |  578 | `	}` |
|    661326 |  579 | `	return SXRET_OK;` |
|         5 |  580 | `}` |
|         - |  581 | `/*` |
|         - |  582 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  583 | ` * Invalidate any prior representations` |
|         - |  584 | ` */` |
|     13332 |  585 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  586 | `{` |
|     13337 |  587 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  588 | `		/* Preform the conversion */` |
|     12153 |  589 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  590 | `		/* Invalidate any prior representations */` |
|     12153 |  591 | `		SyBlobRelease(&pObj->sBlob);` |
|     12153 |  592 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  593 | `		/* Try to get an integer representation */` |
|     12153 |  594 | `		MemObjTryIntger(&(*pObj));` |
|      6074 |  595 | `	}` |
|     13337 |  596 | `	return SXRET_OK;` |
|         5 |  597 | `}` |
|         - |  598 | `/*` |
|         - |  599 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  600 | ` */` |
|     68136 |  601 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  602 | `{` |
|     68141 |  603 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  604 | `		/* Preform the conversion */` |
|     61997 |  605 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  606 | `		/* Invalidate any prior representations */` |
|     61997 |  607 | `		SyBlobRelease(&pObj->sBlob);` |
|     61997 |  608 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     30996 |  609 | `	}` |
|     68141 |  610 | `	return SXRET_OK;` |
|         5 |  611 | `}` |
|         - |  612 | `/*` |
|         - |  613 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  614 | ` */` |
|   1232251 |  615 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  616 | `{` |
|   1232256 |  617 | `	sxi32 rc = SXRET_OK;` |
|   1232256 |  618 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  619 | `		/* Perform the conversion */` |
|     64973 |  620 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     64973 |  621 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     64973 |  622 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  623 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  624 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  625 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  626 | `			 * leaves $o holding the object after the throw is caught. */` |
|       145 |  627 | `			return rc;` |
|         - |  628 | `		}` |
|     64833 |  629 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     32414 |  630 | `	}` |
|   1232116 |  631 | `	return rc;` |
|    616875 |  632 | `}` |
|         - |  633 | `/*` |
|         - |  634 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  635 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  636 | ` *   Error: Object of class X could not be converted to string` |
|         - |  637 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  638 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  639 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  640 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  641 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  642 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  643 | ` * cast still carried the placeholder.` |
|         - |  644 | ` *` |
|         - |  645 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  646 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - |  647 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - |  648 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - |  649 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - |  650 | ` * settype() site then blanks its target itself, because php's` |
|         - |  651 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - |  652 | ` */` |
|       544 |  653 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         3 |  654 | `{` |
|       547 |  655 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - |  656 | `	SyBlob sMsg;` |
|       547 |  657 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       547 |  658 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       544 |  659 | `		&pInst->pClass->sName);` |
|         - |  660 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       547 |  661 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         3 |  662 | `}` |
|         - |  663 | `/*` |
|         - |  664 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - |  665 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - |  666 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - |  667 | ` */` |
|     66170 |  668 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 |  669 | `{` |
|         - |  670 | `	ph7_class_instance *pInst;` |
|     66175 |  671 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|     65079 |  672 | `		return FALSE;` |
|         - |  673 | `	}` |
|      1101 |  674 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      1101 |  675 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 |  676 | `		return FALSE;` |
|         - |  677 | `	}` |
|      1101 |  678 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     33090 |  679 | `}` |
|         - |  680 | `/*` |
|         - |  681 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  682 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  683 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  684 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  685 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  686 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  687 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  688 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  689 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  690 | ` *` |
|         - |  691 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  692 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - |  693 | ` * ph7_value carries.` |
|         - |  694 | ` *` |
|         - |  695 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - |  696 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - |  697 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - |  698 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - |  699 | ` * exactly as php never throws for them.` |
|         - |  700 | ` *` |
|         - |  701 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - |  702 | ` */` |
|    744132 |  703 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  704 | `{` |
|    744137 |  705 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|    680041 |  706 | `		return SXRET_OK;` |
|         - |  707 | `	}` |
|     64101 |  708 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|        85 |  709 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        41 |  710 | `	}` |
|     64101 |  711 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       547 |  712 | `		return MemObjThrowNotStringable(pObj);` |
|         - |  713 | `	}` |
|     63557 |  714 | `	return PH7_MemObjToString(pObj);` |
|    372070 |  715 | `}` |
|         - |  716 | `/*` |
|         - |  717 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  718 | ` * representation.` |
|         - |  719 | ` */` |
|         2 |  720 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  721 | `{` |
|         3 |  722 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  723 | `}` |
|         - |  724 | `/*` |
|         - |  725 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  726 | `  * According to the PHP language reference manual.` |
|         - |  727 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  728 | `  *   to an array results in an array with a single element with index zero` |
|         - |  729 | `  *   and the value of the scalar which was converted.` |
|         - |  730 | `  */` |
|      1084 |  731 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  732 | `{` |
|      1089 |  733 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  734 | `		ph7_hashmap *pMap;` |
|         - |  735 | `		/* Allocate a new hashmap instance */` |
|       721 |  736 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|       721 |  737 | `		if( pMap == 0 ){` |
|       ! 0 |  738 | `			return SXERR_MEM;` |
|         - |  739 | `		}` |
|       721 |  740 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  741 | `			/*` |
|         - |  742 | `			 * According to the PHP language reference manual.` |
|         - |  743 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  744 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  745 | `			 *   and the value of the scalar which was converted.` |
|         - |  746 | `			 */` |
|        51 |  747 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  748 | `				/* Object cast */` |
|        35 |  749 | `				PH7_ClassInstanceToHashmap((ph7_class_instance *)pObj->x.pOther,pMap);` |
|        20 |  750 | `			}else{` |
|         - |  751 | `				/* Insert a single element */` |
|        18 |  752 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  753 | `			}` |
|        51 |  754 | `			SyBlobRelease(&pObj->sBlob);` |
|        23 |  755 | `		}` |
|         - |  756 | `		/* Invalidate any prior representation */` |
|       721 |  757 | `		PH7_MemObjRelease(pObj);` |
|       721 |  758 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|       721 |  759 | `		pObj->x.pOther = pMap;` |
|       358 |  760 | `	}` |
|      1089 |  761 | `	return SXRET_OK;` |
|       547 |  762 | `}` |
|         - |  763 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  764 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  765 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  766 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|        72 |  767 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         3 |  768 | `{` |
|        75 |  769 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  770 | `	ph7_value *pSlot;` |
|         - |  771 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  772 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  773 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  774 | `	 * safe to coerce in place. */` |
|        75 |  775 | `	PH7_MemObjToString(pKey);` |
|       111 |  776 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|        72 |  777 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|        75 |  778 | `	if( pSlot ){` |
|        75 |  779 | `		PH7_MemObjStore(pValue,pSlot);` |
|        36 |  780 | `	}` |
|        75 |  781 | `	return SXRET_OK;` |
|         3 |  782 | `}` |
|         - |  783 | `/*` |
|         - |  784 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  785 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  786 | ` * matching PHP's (object) cast:` |
|         - |  787 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  788 | ` *   - scalar -> a single property named "scalar".` |
|         - |  789 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  790 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  791 | ` */` |
|        58 |  792 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         3 |  793 | `{` |
|        61 |  794 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  795 | `		ph7_class_instance *pStd;` |
|         - |  796 | `		ph7_class *pClass;` |
|         - |  797 | `		ph7_vm *pVm;` |
|         - |  798 | `		/* Point to the underlying VM + the stdClass */` |
|        61 |  799 | `		pVm = pObj->pVm;` |
|        90 |  800 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        29 |  801 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        61 |  802 | `		if( pClass == 0 ){` |
|         - |  803 | `			/* Can't happen,load null instead */` |
|       ! 0 |  804 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  805 | `			return SXRET_OK;` |
|         - |  806 | `		}` |
|         - |  807 | `		/* Instanciate a new (empty) stdClass object */` |
|        61 |  808 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|        61 |  809 | `		if( pStd == 0 ){` |
|         - |  810 | `			/* Out of memory */` |
|       ! 0 |  811 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  812 | `			return SXRET_OK;` |
|         - |  813 | `		}` |
|        61 |  814 | `		pStd->iRef = 1;` |
|        61 |  815 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - |  816 | `			/* Array: one dynamic property per entry. */` |
|         - |  817 | `			struct VmObjCastData sData;` |
|        47 |  818 | `			sData.pVm = pVm;` |
|        47 |  819 | `			sData.pStd = pStd;` |
|        47 |  820 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        38 |  821 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - |  822 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 |  823 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 |  824 | `			if( pSlot ){` |
|        14 |  825 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 |  826 | `			}` |
|         6 |  827 | `		}` |
|         - |  828 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - |  829 | `		/* Invalidate any prior representation */` |
|        61 |  830 | `		PH7_MemObjRelease(pObj);` |
|         - |  831 | `		/* Save the new instance */` |
|        61 |  832 | `		pObj->x.pOther = pStd;` |
|        61 |  833 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        29 |  834 | `	}` |
|        61 |  835 | `	return SXRET_OK;` |
|        32 |  836 | `}` |
|         - |  837 | `/*` |
|         - |  838 | ` * Return a pointer to the appropriate convertion method associated` |
|         - |  839 | ` * with the given type.` |
|         - |  840 | ` * Note on type juggling.` |
|         - |  841 | ` * Accoding to the PHP language reference manual` |
|         - |  842 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - |  843 | ` *  declaration; a variable's type is determined by the context in which` |
|         - |  844 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - |  845 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - |  846 | ` *  assigned to $var, it becomes an integer.` |
|         - |  847 | ` */` |
|    100198 |  848 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 |  849 | `{` |
|    100203 |  850 | `	if( iFlags & MEMOBJ_STRING ){` |
|        94 |  851 | `		return PH7_MemObjToString;` |
|    100113 |  852 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100093 |  853 | `		return PH7_MemObjToInteger;` |
|        23 |  854 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        16 |  855 | `		return PH7_MemObjToReal;` |
|         8 |  856 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|       ! 0 |  857 | `		return PH7_MemObjToBool;` |
|         8 |  858 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 |  859 | `		return PH7_MemObjToHashmap;` |
|       ! 0 |  860 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  861 | `		return PH7_MemObjToObject;` |
|       ! 0 |  862 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  863 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - |  864 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - |  865 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - |  866 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - |  867 | `		 * default. */` |
|       ! 0 |  868 | `		return 0;` |
|         - |  869 | `	}` |
|         - |  870 | `	/* NULL cast */` |
|       ! 0 |  871 | `	return PH7_MemObjToNull;` |
|     50104 |  872 | `}` |
|         - |  873 | `/*` |
|         - |  874 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - |  875 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - |  876 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - |  877 | ` * loose-comparison numeric gate:` |
|         - |  878 | ` *` |
|         - |  879 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - |  880 | ` *` |
|         - |  881 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - |  882 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - |  883 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - |  884 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - |  885 | ` * a non-string value.` |
|         - |  886 | ` */` |
|         - |  887 | `/*` |
|         - |  888 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - |  889 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - |  890 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - |  891 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - |  892 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - |  893 | ` * and rejects a string with no prefix outright.` |
|         - |  894 | ` */` |
|    387977 |  895 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 |  896 | `{` |
|         - |  897 | `	const char *z, *zEnd;` |
|         - |  898 | `	sxu32 n;` |
|    387982 |  899 | `	int bDigit = 0;` |
|    387982 |  900 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  901 | `		return 0;` |
|         - |  902 | `	}` |
|    387982 |  903 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|    387982 |  904 | `	n = SyBlobLength(&pValue->sBlob);` |
|    387982 |  905 | `	if( n == 0 ){` |
|       631 |  906 | `		return 0;` |
|         - |  907 | `	}` |
|    387354 |  908 | `	zEnd = z + n;` |
|    387558 |  909 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       207 |  910 | `		z++;` |
|         3 |  911 | `	}` |
|    387354 |  912 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       637 |  913 | `		z++;` |
|       316 |  914 | `	}` |
|    463744 |  915 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     76395 |  916 | `		z++; bDigit = 1;` |
|         5 |  917 | `	}` |
|    387354 |  918 | `	if( z < zEnd && z[0] == '.' ){` |
|      6851 |  919 | `		z++;` |
|      7887 |  920 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1039 |  921 | `			z++; bDigit = 1;` |
|         3 |  922 | `		}` |
|      3572 |  923 | `	}` |
|         - |  924 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|    387354 |  925 | `	if( !bDigit ){` |
|    375788 |  926 | `		return 0;` |
|         - |  927 | `	}` |
|         - |  928 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - |  929 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|     11571 |  930 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       136 |  931 | `		const char *zExp = z;` |
|       136 |  932 | `		z++;` |
|       136 |  933 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       ! 0 |  934 | `			z++;` |
|       ! 0 |  935 | `		}` |
|       136 |  936 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        10 |  937 | `			z = zExp;` |
|         6 |  938 | `		}else{` |
|       294 |  939 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       168 |  940 | `				z++;` |
|         2 |  941 | `			}` |
|         - |  942 | `		}` |
|        67 |  943 | `	}` |
|     11571 |  944 | `	if( pzTail ){` |
|     11571 |  945 | `		*pzTail = z;` |
|      5783 |  946 | `	}` |
|     11571 |  947 | `	return 1;` |
|    193845 |  948 | `}` |
|         - |  949 | `/*` |
|         - |  950 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - |  951 | ` * (trailing whitespace allowed, nothing else).` |
|         - |  952 | ` */` |
|    382161 |  953 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 |  954 | `{` |
|    382166 |  955 | `	const char *zTail = 0, *zEnd;` |
|    382166 |  956 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    376360 |  957 | `		return 0;` |
|         - |  958 | `	}` |
|      5811 |  959 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      5835 |  960 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        26 |  961 | `		zTail++;` |
|         2 |  962 | `	}` |
|      5811 |  963 | `	return zTail == zEnd ? 1 : 0;` |
|    190937 |  964 | `}` |
|         - |  965 | `/*` |
|         - |  966 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - |  967 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - |  968 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - |  969 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - |  970 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - |  971 | ` * those bytes convert to when it did.` |
|         - |  972 | ` *` |
|         - |  973 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - |  974 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - |  975 | ` * however large it is, because it was always going to be a double, so making it` |
|         - |  976 | ` * one lost no digits.` |
|         - |  977 | ` *` |
|         - |  978 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - |  979 | ` * intact when this says no.` |
|         - |  980 | ` */` |
|      2788 |  981 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         3 |  982 | `{` |
|      2791 |  983 | `	const char *z, *zTail = 0;` |
|      2791 |  984 | `	int iOverflow = 0;` |
|      2791 |  985 | `	*piOverflow = 0;` |
|      2791 |  986 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|        96 |  987 | `		return FALSE;` |
|         - |  988 | `	}` |
|      2697 |  989 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 |  990 | `		return FALSE;` |
|         - |  991 | `	}` |
|         - |  992 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - |  993 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|      2697 |  994 | `	z = (const char *)SyBlobData(&pObj->sBlob);` |
|     24625 |  995 | `	while( z < zTail ){` |
|     22011 |  996 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|        82 |  997 | `			return FALSE;` |
|         - |  998 | `		}` |
|     21931 |  999 | `		z++;` |
|         3 | 1000 | `	}` |
|      2617 | 1001 | `	MemObjStringToInt(pObj,&iOverflow);` |
|      2617 | 1002 | `	*piOverflow = iOverflow;` |
|      2617 | 1003 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1004 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1005 | `	}` |
|      2617 | 1006 | `	return TRUE;` |
|      1397 | 1007 | `}` |
|         - | 1008 | `/*` |
|         - | 1009 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1010 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1011 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1012 | ` */` |
|    280955 | 1013 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1014 | `{` |
|    280960 | 1015 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       929 | 1016 | `		return TRUE;` |
|    280036 | 1017 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       791 | 1018 | `		return FALSE;` |
|    279250 | 1019 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1020 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    279250 | 1021 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1022 | `	}` |
|         - | 1023 | `	/* NOT REACHED */` |
|       ! 0 | 1024 | `	return FALSE;` |
|    140334 | 1025 | `}` |
|         - | 1026 | `/*` |
|         - | 1027 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1028 | ` * FALSE otherwise.` |
|         - | 1029 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1030 | ` * NULL value.` |
|         - | 1031 | ` * Boolean FALSE.` |
|         - | 1032 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1033 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1034 | ` * An empty array.` |
|         - | 1035 | ` * NOTE` |
|         - | 1036 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1037 | ` */` |
|     42990 | 1038 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1039 | `{` |
|     42995 | 1040 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        36 | 1041 | `		return TRUE;` |
|     42963 | 1042 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|        22 | 1043 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|     42943 | 1044 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 1045 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|     42943 | 1046 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|         5 | 1047 | `		return !pObj->x.iVal;` |
|     42939 | 1048 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     28559 | 1049 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|     22483 | 1050 | `			return TRUE;` |
|       ! 0 | 1051 | `		}else{` |
|         - | 1052 | `			const char *zIn,*zEnd;` |
|      6081 | 1053 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|      6081 | 1054 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|      6089 | 1055 | `			while( zIn < zEnd ){` |
|      6089 | 1056 | `				if( zIn[0] != '0' ){` |
|      6081 | 1057 | `					break;` |
|         - | 1058 | `				}` |
|        10 | 1059 | `				zIn++;` |
|         2 | 1060 | `			}` |
|      6081 | 1061 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|       ! 0 | 1062 | `		}` |
|     14385 | 1063 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     14385 | 1064 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     14385 | 1065 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|       ! 0 | 1066 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       ! 0 | 1067 | `		return FALSE;` |
|         - | 1068 | `	}` |
|         - | 1069 | `	/* Assume empty by default */` |
|       ! 0 | 1070 | `	return TRUE;` |
|     21500 | 1071 | `}` |
|         - | 1072 | `/*` |
|         - | 1073 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1074 | ` * or both.` |
|         - | 1075 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1076 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1077 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1078 | ` * and ignore the rest.` |
|         - | 1079 | ` */` |
|    853474 | 1080 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1081 | `{` |
|    853479 | 1082 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|    850687 | 1083 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        26 | 1084 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        20 | 1085 | `				pObj->x.iVal = 0;` |
|         8 | 1086 | `			}` |
|        26 | 1087 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        11 | 1088 | `		}` |
|         - | 1089 | `		/* Already numeric */` |
|    850687 | 1090 | `		return  SXRET_OK;` |
|         - | 1091 | `	}` |
|      2797 | 1092 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      2797 | 1093 | `		const char *zTail = 0;` |
|      2797 | 1094 | `		int bNum, bReal = 0;` |
|         - | 1095 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1096 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1097 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1098 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      2797 | 1099 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      2797 | 1100 | `		if( bNum ){` |
|      2797 | 1101 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     10193 | 1102 | `			while( z < zTail ){` |
|      7517 | 1103 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       118 | 1104 | `					bReal = 1;` |
|       118 | 1105 | `					break;` |
|         - | 1106 | `				}` |
|      7401 | 1107 | `				z++;` |
|         5 | 1108 | `			}` |
|      1396 | 1109 | `		}` |
|      2797 | 1110 | `		if( bReal ){` |
|       118 | 1111 | `			PH7_MemObjToReal(&(*pObj));` |
|        60 | 1112 | `		}else{` |
|      2681 | 1113 | `			if( !bNum ){` |
|         - | 1114 | `				/* The input does not look at all like a number,set the value to 0 */` |
|       ! 0 | 1115 | `				pObj->x.iVal = 0;` |
|       ! 0 | 1116 | `			}else{` |
|      2681 | 1117 | `				int iOverflow = 0;` |
|         - | 1118 | `				/* Convert as much as we can */` |
|      2681 | 1119 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1120 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      2681 | 1121 | `				if( iOverflow ){` |
|         - | 1122 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1123 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1124 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1125 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1126 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1127 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1128 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1129 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1130 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1131 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1132 | `					 * OP_ADD's overflow arm makes there. */` |
|       215 | 1133 | `					PH7_MemObjToReal(&(*pObj));` |
|       215 | 1134 | `					return SXRET_OK;` |
|         - | 1135 | `				}` |
|         - | 1136 | `#endif` |
|         - | 1137 | `			}` |
|      2467 | 1138 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      2467 | 1139 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1140 | `		}` |
|      1289 | 1141 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 | 1142 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1143 | `	}else{` |
|         - | 1144 | `		/* Perform a blind cast */` |
|       ! 0 | 1145 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1146 | `	}` |
|      2583 | 1147 | `	return SXRET_OK;` |
|    427249 | 1148 | `}` |
|         - | 1149 | `/*` |
|         - | 1150 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1151 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1152 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1153 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1154 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1155 | ` * last carried character. Empty strings become "1".` |
|         - | 1156 | ` *` |
|         - | 1157 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1158 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1159 | ` * a string even though it looks numeric.` |
|         - | 1160 | ` */` |
|       ! 0 | 1161 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1162 | `{` |
|         - | 1163 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1164 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1165 | `	sxu32 nLen, pos;` |
|         - | 1166 | `	sxu8 *zStr;` |
|       ! 0 | 1167 | `	int carry = 1;` |
|         - | 1168 | `	int ch;` |
|         - | 1169 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1170 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1171 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1172 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1173 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1174 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1175 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1176 | `	}` |
|       ! 0 | 1177 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1178 | `	if( nLen == 0 ){` |
|       ! 0 | 1179 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1180 | `		return SXRET_OK;` |
|         - | 1181 | `	}` |
|       ! 0 | 1182 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1183 | `	pos = nLen;` |
|       ! 0 | 1184 | `	while( pos > 0 ){` |
|       ! 0 | 1185 | `		pos--;` |
|       ! 0 | 1186 | `		ch = zStr[pos];` |
|       ! 0 | 1187 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1188 | `			if( ch == 'z' ){` |
|       ! 0 | 1189 | `				zStr[pos] = 'a';` |
|       ! 0 | 1190 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1191 | `				continue;` |
|         - | 1192 | `			}` |
|       ! 0 | 1193 | `			zStr[pos]++;` |
|       ! 0 | 1194 | `			carry = 0;` |
|       ! 0 | 1195 | `			break;` |
|       ! 0 | 1196 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1197 | `			if( ch == 'Z' ){` |
|       ! 0 | 1198 | `				zStr[pos] = 'A';` |
|       ! 0 | 1199 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1200 | `				continue;` |
|         - | 1201 | `			}` |
|       ! 0 | 1202 | `			zStr[pos]++;` |
|       ! 0 | 1203 | `			carry = 0;` |
|       ! 0 | 1204 | `			break;` |
|       ! 0 | 1205 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1206 | `			if( ch == '9' ){` |
|       ! 0 | 1207 | `				zStr[pos] = '0';` |
|       ! 0 | 1208 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1209 | `				continue;` |
|         - | 1210 | `			}` |
|       ! 0 | 1211 | `			zStr[pos]++;` |
|       ! 0 | 1212 | `			carry = 0;` |
|       ! 0 | 1213 | `			break;` |
|       ! 0 | 1214 | `		}else{` |
|         - | 1215 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1216 | `			carry = 0;` |
|       ! 0 | 1217 | `			break;` |
|         - | 1218 | `		}` |
|       ! 0 | 1219 | `	}` |
|       ! 0 | 1220 | `	if( carry ){` |
|         - | 1221 | `		sxu8 prepend;` |
|         - | 1222 | `		sxu32 i;` |
|       ! 0 | 1223 | `		switch( last_class ){` |
|       ! 0 | 1224 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1225 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1226 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1227 | `		}` |
|         - | 1228 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1229 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1230 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1231 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1232 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1233 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1234 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1235 | `		}` |
|       ! 0 | 1236 | `		zStr[0] = prepend;` |
|       ! 0 | 1237 | `	}` |
|       ! 0 | 1238 | `	return SXRET_OK;` |
|       ! 0 | 1239 | `}` |
|         - | 1240 | `/*` |
|         - | 1241 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1242 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1243 | ` */` |
|      1238 | 1244 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         5 | 1245 | `{` |
|      1243 | 1246 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1247 | `		/* Work only with reals */` |
|      1243 | 1248 | `		MemObjTryIntger(&(*pObj));` |
|       619 | 1249 | `	}` |
|      1243 | 1250 | `	return SXRET_OK;` |
|         5 | 1251 | `}` |
|         - | 1252 | `/*` |
|         - | 1253 | ` * Initialize a ph7_value to the null type.` |
|         - | 1254 | ` */` |
| 248744193 | 1255 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1256 | `{` |
|         - | 1257 | `	/* Zero the structure */` |
| 248744198 | 1258 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1259 | `	/* Initialize fields */` |
| 248744198 | 1260 | `	pObj->pVm = pVm;` |
| 248744198 | 1261 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1262 | `	/* Set the NULL type */` |
| 248744198 | 1263 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 248744198 | 1264 | `	return SXRET_OK;` |
|         5 | 1265 | `}` |
|         - | 1266 | `/*` |
|         - | 1267 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1268 | ` */` |
|   8558978 | 1269 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1270 | `{` |
|         - | 1271 | `	/* Zero the structure */` |
|   8558983 | 1272 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1273 | `	/* Initialize fields */` |
|   8558983 | 1274 | `	pObj->pVm = pVm;` |
|   8558983 | 1275 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1276 | `	/* Set the desired type */` |
|   8558983 | 1277 | `	pObj->x.iVal = iVal;` |
|   8558983 | 1278 | `	pObj->iFlags = MEMOBJ_INT;` |
|   8558983 | 1279 | `	return SXRET_OK;` |
|         5 | 1280 | `}` |
|         - | 1281 | `/*` |
|         - | 1282 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1283 | ` */` |
|     18444 | 1284 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1285 | `{` |
|         - | 1286 | `	/* Zero the structure */` |
|     18449 | 1287 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1288 | `	/* Initialize fields */` |
|     18449 | 1289 | `	pObj->pVm = pVm;` |
|     18449 | 1290 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1291 | `	/* Set the desired type */` |
|     18449 | 1292 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     18449 | 1293 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     18449 | 1294 | `	return SXRET_OK;` |
|         5 | 1295 | `}` |
|         - | 1296 | `/*` |
|         - | 1297 | ` * Initialize a ph7_value to the real type.` |
|         - | 1298 | ` */` |
|        16 | 1299 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         2 | 1300 | `{` |
|         - | 1301 | `	/* Zero the structure */` |
|        18 | 1302 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1303 | `	/* Initialize fields */` |
|        18 | 1304 | `	pObj->pVm = pVm;` |
|        18 | 1305 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1306 | `	/* Set the desired type */` |
|        18 | 1307 | `	pObj->rVal = rVal;` |
|        18 | 1308 | `	pObj->iFlags = MEMOBJ_REAL;` |
|        18 | 1309 | `	return SXRET_OK;` |
|         2 | 1310 | `}` |
|         - | 1311 | `/*` |
|         - | 1312 | ` * Initialize a ph7_value to the array type.` |
|         - | 1313 | ` */` |
|   2144938 | 1314 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1315 | `{` |
|         - | 1316 | `	/* Zero the structure */` |
|   2144943 | 1317 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1318 | `	/* Initialize fields */` |
|   2144943 | 1319 | `	pObj->pVm = pVm;` |
|   2144943 | 1320 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1321 | `	/* Set the desired type */` |
|   2144943 | 1322 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   2144943 | 1323 | `	pObj->x.pOther = pArray;` |
|   2144943 | 1324 | `	return SXRET_OK;` |
|         5 | 1325 | `}` |
|         - | 1326 | `/*` |
|         - | 1327 | ` * Initialize a ph7_value to the string type.` |
|         - | 1328 | ` */` |
|  10473046 | 1329 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1330 | `{` |
|         - | 1331 | `	/* Zero the structure */` |
|  10473051 | 1332 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1333 | `	/* Initialize fields */` |
|  10473051 | 1334 | `	pObj->pVm = pVm;` |
|  10473051 | 1335 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  10473051 | 1336 | `	if( pVal ){` |
|         - | 1337 | `		/* Append contents */` |
|   4719707 | 1338 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   2359851 | 1339 | `	}` |
|         - | 1340 | `	/* Set the desired type */` |
|  10473051 | 1341 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  10473051 | 1342 | `	return SXRET_OK;` |
|         5 | 1343 | `}` |
|         - | 1344 | `/*` |
|         - | 1345 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1346 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1347 | ` * invalidate any prior representation and set the string type.` |
|         - | 1348 | ` * Then a simple append operation is performed.` |
|         - | 1349 | ` */` |
|   6245285 | 1350 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1351 | `{` |
|         - | 1352 | `	sxi32 rc;` |
|   6245290 | 1353 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1354 | `		/* Invalidate any prior representation */` |
|      9291 | 1355 | `		PH7_MemObjRelease(pObj);` |
|      9291 | 1356 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      4643 | 1357 | `	}` |
|         - | 1358 | `	/* Append contents */` |
|   6245290 | 1359 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   6245290 | 1360 | `	return rc;` |
|         5 | 1361 | `}` |
|         - | 1362 | `#if 0` |
|         - | 1363 | `/*` |
|         - | 1364 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1365 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1366 | ` * any prior representation and set the string type.` |
|         - | 1367 | ` * Then a simple format and append operation is performed.` |
|         - | 1368 | ` */` |
|         - | 1369 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1370 | `{` |
|         - | 1371 | `	sxi32 rc;` |
|         - | 1372 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1373 | `		/* Invalidate any prior representation */` |
|         - | 1374 | `		PH7_MemObjRelease(pObj);` |
|         - | 1375 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1376 | `	}` |
|         - | 1377 | `	/* Format and append contents */` |
|         - | 1378 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1379 | `	return rc;` |
|         - | 1380 | `}` |
|         - | 1381 | `#endif` |
|         - | 1382 | `/*` |
|         - | 1383 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1384 | ` */` |
|  27972159 | 1385 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1386 | `{` |
|  27972164 | 1387 | `	ph7_class_instance *pObj = 0;` |
|  27972164 | 1388 | `	ph7_hashmap *pMap = 0;` |
|         - | 1389 | `	sxi32 rc;` |
|  27972164 | 1390 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1391 | `		/* Increment reference count */` |
|   2317761 | 1392 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  26813286 | 1393 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1394 | `		/* Increment reference count */` |
|     16633 | 1395 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|      8314 | 1396 | `	}` |
|  27972164 | 1397 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|     90825 | 1398 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  27926754 | 1399 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|      5365 | 1400 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      2680 | 1401 | `	}` |
|  27972164 | 1402 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  27972164 | 1403 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  27972164 | 1404 | `	rc = SXRET_OK;` |
|  27972164 | 1405 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|   9858726 | 1406 | `		SyBlobReset(&pDest->sBlob);` |
|   9858726 | 1407 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   4930260 | 1408 | `	}else{` |
|  18113443 | 1409 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   1873218 | 1410 | `			SyBlobRelease(&pDest->sBlob);` |
|    937153 | 1411 | `		}` |
|         - | 1412 | `	}` |
|  27972164 | 1413 | `	if( pMap ){` |
|     90825 | 1414 | `		PH7_HashmapUnref(pMap);` |
|  27926754 | 1415 | `	}else if( pObj ){` |
|      5365 | 1416 | `		PH7_ClassInstanceUnref(pObj);` |
|      2680 | 1417 | `	}` |
|  27972159 | 1418 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  15147689 | 1419 | `	 && pDest->pVm` |
|   2317756 | 1420 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1421 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1422 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1423 | `	  * for closure envs and other non-slot destinations. */` |
|   1158887 | 1424 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1425 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1426 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1427 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1428 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1429 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1430 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1431 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1432 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1433 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1434 | `			pDest->x.pOther = pSnap;` |
|         4 | 1435 | `		}else if( pSnap ){` |
|       ! 0 | 1436 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1437 | `		}` |
|         4 | 1438 | `	}` |
|  27972164 | 1439 | `	return rc;` |
|         5 | 1440 | `}` |
|         - | 1441 | `/*` |
|         - | 1442 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1443 | ` * buffer contents,simply point to it.` |
|         - | 1444 | ` */` |
|  40291330 | 1445 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1446 | `{` |
|  40291335 | 1447 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1448 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1449 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1450 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1451 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1452 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1453 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  40291335 | 1454 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  40291335 | 1455 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1456 | `		/* Increment reference count */` |
|    733347 | 1457 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  39924664 | 1458 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1459 | `		/* Increment reference count */` |
|   3356321 | 1460 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|   1678158 | 1461 | `	}` |
|  40291335 | 1462 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       107 | 1463 | `		SyBlobRelease(&pDest->sBlob);` |
|        51 | 1464 | `	}` |
|  40291335 | 1465 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  18523384 | 1466 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   9264385 | 1467 | `	}` |
|  40291335 | 1468 | `	return SXRET_OK;` |
|         5 | 1469 | `}` |
|         - | 1470 | `/*` |
|         - | 1471 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1472 | ` */` |
| 184159273 | 1473 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1474 | `{` |
| 184159278 | 1475 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1476 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1477 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1478 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1479 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1480 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1481 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1482 | `		pObj->x.pOther = 0;` |
|         7 | 1483 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1484 | `	}` |
| 184159278 | 1485 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1486 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1487 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1488 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1489 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|       ! 0 | 1490 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|       ! 0 | 1491 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1492 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|       ! 0 | 1493 | `	}` |
| 184159278 | 1494 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  84391205 | 1495 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   4964689 | 1496 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  81908863 | 1497 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   9437271 | 1498 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   4718633 | 1499 | `		}` |
|         - | 1500 | `		/* Release the internal buffer */` |
|  84391205 | 1501 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1502 | `		/* Invalidate any prior representation */` |
|  84391205 | 1503 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  42205286 | 1504 | `	}` |
| 184159278 | 1505 | `	return SXRET_OK;` |
|         5 | 1506 | `}` |
|         - | 1507 | `/*` |
|         - | 1508 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1509 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1510 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1511 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1512 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1513 | ` *` |
|         - | 1514 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1515 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1516 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1517 | ` *` |
|         - | 1518 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1519 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1520 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1521 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1522 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1523 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1524 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1525 | ` */` |
|       106 | 1526 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         2 | 1527 | `{` |
|       108 | 1528 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       108 | 1529 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       108 | 1530 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|        66 | 1531 | `		if( PH7_MemObjIsNotStringable(pSelf) ){` |
|        14 | 1532 | `			return FALSE;` |
|         - | 1533 | `		}` |
|        53 | 1534 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        53 | 1535 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1536 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1537 | `			 * point; until then order the operands the way a refused cast does. */` |
|       ! 0 | 1538 | `			return FALSE;` |
|         - | 1539 | `		}` |
|        53 | 1540 | `		return TRUE;` |
|         - | 1541 | `	}` |
|        43 | 1542 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1543 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|         7 | 1544 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|         7 | 1545 | `		return TRUE;` |
|         - | 1546 | `	}` |
|        37 | 1547 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1548 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1549 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1550 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1551 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1552 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|        10 | 1553 | `		}` |
|        21 | 1554 | `		if( bReal ){` |
|         7 | 1555 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 1556 | `		}else{` |
|        15 | 1557 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 1558 | `		}` |
|        21 | 1559 | `		return TRUE;` |
|         - | 1560 | `	}` |
|        17 | 1561 | `	return FALSE;` |
|        55 | 1562 | `}` |
|         - | 1563 | `/*` |
|         - | 1564 | ` * Compare two ph7_values.` |
|         - | 1565 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1566 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1567 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1568 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1569 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1570 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1571 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1572 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1573 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1574 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1575 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1576 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1577 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1578 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1579 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1580 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1581 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1582 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1583 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1584 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1585 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1586 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1587 | ` *      Loose comparisons with ==` |
|         - | 1588 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1589 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1590 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1591 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1592 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1593 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1594 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1595 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1596 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1597 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1598 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1599 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1600 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1601 | ` *    Strict comparisons with ===` |
|         - | 1602 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1603 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1604 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1605 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1606 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1607 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1608 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1609 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1610 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1611 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1612 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1613 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1614 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1615 | ` */` |
|   2155927 | 1616 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	sxi32 iComb;` |
|         - | 1619 | `	sxi32 rc;` |
|   2155932 | 1620 | `	if( bStrict ){` |
|         - | 1621 | `		sxi32 iF1,iF2;` |
|         - | 1622 | `		/* Strict comparisons with === */` |
|   1091724 | 1623 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   1091724 | 1624 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   1091724 | 1625 | `		if( iF1 != iF2 ){` |
|         - | 1626 | `			/* Not of the same type */` |
|    242451 | 1627 | `			return 1;` |
|         - | 1628 | `		}` |
|    425041 | 1629 | `	}` |
|         - | 1630 | `	/* Combine flag together */` |
|   1913486 | 1631 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   1913481 | 1632 | `	if( !bStrict` |
|   1489249 | 1633 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    532599 | 1634 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        68 | 1635 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1636 | `		/*` |
|         - | 1637 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1638 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1639 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1640 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1641 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1642 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1643 | `		 */` |
|        45 | 1644 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        35 | 1645 | `			PH7_MemObjToString(pObj1);` |
|        18 | 1646 | `		}else{` |
|        11 | 1647 | `			PH7_MemObjToString(pObj2);` |
|         - | 1648 | `		}` |
|        45 | 1649 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1650 | `	}` |
|   1913486 | 1651 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1652 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1653 | `		 * call every live resource equal to every other, since all are truthy. */` |
|         5 | 1654 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|         5 | 1655 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|         5 | 1656 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1657 | `	}` |
|   1913482 | 1658 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 1659 | `		/*` |
|         - | 1660 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 1661 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 1662 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 1663 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 1664 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 1665 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 1666 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 1667 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 1668 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 1669 | `		 * equalled every open resource.` |
|         - | 1670 | `		 *` |
|         - | 1671 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 1672 | `		 * already answered 1.` |
|         - | 1673 | `		 */` |
|       108 | 1674 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       108 | 1675 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       108 | 1676 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 1677 | `		ph7_value sCast;` |
|       108 | 1678 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 1679 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        71 | 1680 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        47 | 1681 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|        79 | 1682 | `			PH7_MemObjRelease(&sCast);` |
|        79 | 1683 | `			return rc;` |
|         - | 1684 | `		}` |
|        30 | 1685 | `		PH7_MemObjRelease(&sCast);` |
|         - | 1686 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|        30 | 1687 | `		return bObj1 ? 1 : -1;` |
|         - | 1688 | `	}` |
|   1913376 | 1689 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|         - | 1690 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|     56605 | 1691 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     30891 | 1692 | `			PH7_MemObjToBool(pObj1);` |
|     15443 | 1693 | `		}` |
|     56605 | 1694 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     29761 | 1695 | `			PH7_MemObjToBool(pObj2);` |
|     14878 | 1696 | `		}` |
|     56605 | 1697 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   1856776 | 1698 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1699 | `		/* Hashmap aka 'array' comparison */` |
|        76 | 1700 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1701 | `			/* Array is always greater */` |
|       ! 0 | 1702 | `			return -1;` |
|         - | 1703 | `		}` |
|        76 | 1704 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1705 | `			/* Array is always greater */` |
|       ! 0 | 1706 | `			return 1;` |
|         - | 1707 | `		}` |
|         - | 1708 | `		/* Perform the comparison */` |
|        76 | 1709 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|        76 | 1710 | `		return rc;` |
|   1856704 | 1711 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1712 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 1713 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 1714 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 1715 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|       309 | 1716 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1717 | `			/* Object is always greater */` |
|       ! 0 | 1718 | `			return -1;` |
|         - | 1719 | `		}` |
|       309 | 1720 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1721 | `			/* Object is always greater */` |
|       ! 0 | 1722 | `			return 1;` |
|         - | 1723 | `		}` |
|         - | 1724 | `		/* Perform the comparison */` |
|       309 | 1725 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       309 | 1726 | `		return rc;` |
|   1856400 | 1727 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 1728 | `		SyString s1,s2;` |
|   1065172 | 1729 | `		if( !bStrict ){` |
|         - | 1730 | `			/*` |
|         - | 1731 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 1732 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 1733 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 1734 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 1735 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 1736 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 1737 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 1738 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 1739 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 1740 | `			 * below, unchanged.` |
|         - | 1741 | `			 */` |
|    276820 | 1742 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 1743 | `				/*` |
|         - | 1744 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 1745 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 1746 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 1747 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 1748 | `				 * really does compare as doubles, so` |
|         - | 1749 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 1750 | `				 *` |
|         - | 1751 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 1752 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 1753 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 1754 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 1755 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 1756 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 1757 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 1758 | `				 *    one, no conversion involved -- which is why` |
|         - | 1759 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 1760 | `				 *    reach the same double.` |
|         - | 1761 | `				 *` |
|         - | 1762 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 1763 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 1764 | `				 */` |
|      1397 | 1765 | `				int bBytes = 0;` |
|         - | 1766 | `				{` |
|      1397 | 1767 | `					ph7_real r1 = 0, r2 = 0;` |
|      1397 | 1768 | `					int iOf1 = 0, iOf2 = 0;` |
|      1397 | 1769 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|      1397 | 1770 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|      1397 | 1771 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 1772 | `						bBytes = 1;` |
|      1347 | 1773 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        35 | 1774 | `						return iOf1;` |
|      1273 | 1775 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        21 | 1776 | `						return -iOf2;` |
|         - | 1777 | `					}` |
|         - | 1778 | `				}` |
|      1353 | 1779 | `				if( !bBytes ){` |
|         - | 1780 | `					/* Perform a numeric comparison */` |
|      1253 | 1781 | `					goto Numeric;` |
|         - | 1782 | `				}` |
|        50 | 1783 | `			}` |
|    137612 | 1784 | `		}` |
|         - | 1785 | `		/* Perform a strict string comparison.*/` |
|   1063878 | 1786 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        31 | 1787 | `			PH7_MemObjToString(pObj1);` |
|        15 | 1788 | `		}` |
|   1063878 | 1789 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        27 | 1790 | `			PH7_MemObjToString(pObj2);` |
|        13 | 1791 | `		}` |
|   1063878 | 1792 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   1063878 | 1793 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 1794 | `		/*` |
|         - | 1795 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 1796 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 1797 | `		 */` |
|   1063878 | 1798 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   1063878 | 1799 | `		if( rc == 0 ){` |
|    339712 | 1800 | `			if( s1.nByte != s2.nByte ){` |
|     21766 | 1801 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     10886 | 1802 | `			}` |
|    169863 | 1803 | `		}` |
|   1063878 | 1804 | `		return rc;` |
|    791233 | 1805 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    395011 | 1806 | `Numeric:` |
|         - | 1807 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|    792483 | 1808 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      1203 | 1809 | `			PH7_MemObjToNumeric(pObj1);` |
|       600 | 1810 | `		}` |
|    792483 | 1811 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      1209 | 1812 | `			PH7_MemObjToNumeric(pObj2);` |
|       603 | 1813 | `		}` |
|    792483 | 1814 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 1815 | `			/*` |
|         - | 1816 | `			 * Symisc eXtension to the PHP language:` |
|         - | 1817 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 1818 | `			 */` |
|         - | 1819 | `			ph7_real r1,r2;` |
|         - | 1820 | `			/* Compare as reals */` |
|       520 | 1821 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        27 | 1822 | `				PH7_MemObjToReal(pObj1);` |
|        13 | 1823 | `			}` |
|       520 | 1824 | `			r1 = pObj1->rVal;` |
|       520 | 1825 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        69 | 1826 | `				PH7_MemObjToReal(pObj2);` |
|        33 | 1827 | `			}` |
|       520 | 1828 | `			r2 = pObj2->rVal;` |
|       520 | 1829 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 1830 | `				/*` |
|         - | 1831 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|         - | 1832 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|         - | 1833 | `				 * any non-NaN numeric value.` |
|         - | 1834 | `				 */` |
|        52 | 1835 | `				if( PH7_IS_NAN(r1) ){` |
|        42 | 1836 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|         - | 1837 | `				}` |
|        11 | 1838 | `				return -1;` |
|         - | 1839 | `			}` |
|       470 | 1840 | `			if( r1 > r2 ){` |
|        84 | 1841 | `				return 1;` |
|       389 | 1842 | `			}else if( r1 < r2 ){` |
|       161 | 1843 | `				return -1;` |
|         - | 1844 | `			}` |
|       230 | 1845 | `			return 0;` |
|       ! 0 | 1846 | `		}else{` |
|         - | 1847 | `			/* Integer comparison */` |
|    791967 | 1848 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|      8507 | 1849 | `				return 1;` |
|    783465 | 1850 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|    763487 | 1851 | `				return -1;` |
|         - | 1852 | `			}` |
|     19983 | 1853 | `			return 0;` |
|         - | 1854 | `		}` |
|         - | 1855 | `	}` |
|         - | 1856 | `	/* NOT REACHED */` |
|       ! 0 | 1857 | `	return 0;` |
|   1078827 | 1858 | `}` |
|         - | 1859 | `/*` |
|         - | 1860 | ` * Perform an addition operation of two ph7_values.` |
|         - | 1861 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 1862 | ` * is that the '+' operator is overloaded.` |
|         - | 1863 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 1864 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 1865 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 1866 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 1867 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 1868 | ` * be ignored.` |
|         - | 1869 | ` * This function take care of handling all the scenarios.` |
|         - | 1870 | ` */` |
|     23974 | 1871 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 1872 | `{` |
|     23979 | 1873 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1874 | `			/* Arithemtic operation */` |
|     19511 | 1875 | `			PH7_MemObjToNumeric(pObj1);` |
|     19511 | 1876 | `			PH7_MemObjToNumeric(pObj2);` |
|     19511 | 1877 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 1878 | `				/* Floating point arithmetic */` |
|         - | 1879 | `				ph7_real a,b;` |
|       116 | 1880 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        29 | 1881 | `					PH7_MemObjToReal(pObj1);` |
|        14 | 1882 | `				}` |
|       116 | 1883 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        53 | 1884 | `					PH7_MemObjToReal(pObj2);` |
|        26 | 1885 | `				}` |
|       116 | 1886 | `				a = pObj1->rVal;` |
|       116 | 1887 | `				b = pObj2->rVal;` |
|       116 | 1888 | `				pObj1->rVal = a+b;` |
|       116 | 1889 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1890 | `				/* Try to get an integer representation also */` |
|       116 | 1891 | `				MemObjTryIntger(&(*pObj1));` |
|        59 | 1892 | `			}else{` |
|         - | 1893 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 1894 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 1895 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 1896 | `				sxi64 a,b,r;` |
|     19397 | 1897 | `				a = pObj1->x.iVal;` |
|     19397 | 1898 | `				b = pObj2->x.iVal;` |
|     19397 | 1899 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 1900 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         9 | 1901 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|         9 | 1902 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1903 | `#else` |
|         - | 1904 | `					pObj1->x.iVal = r;` |
|         - | 1905 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1906 | `#endif` |
|         5 | 1907 | `				}else{` |
|     19389 | 1908 | `					pObj1->x.iVal = r;` |
|     19389 | 1909 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1910 | `				}` |
|         - | 1911 | `			}` |
|      9758 | 1912 | `	}else{` |
|      4473 | 1913 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 1914 | `			ph7_hashmap *pMap;` |
|         - | 1915 | `			sxi32 rc;` |
|      4473 | 1916 | `			if( bAddStore ){` |
|         - | 1917 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 1918 | `				 */` |
|         3 | 1919 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1920 | `					/* Force a hashmap cast */` |
|       ! 0 | 1921 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 1922 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1923 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1924 | `						return rc;` |
|         - | 1925 | `					}` |
|       ! 0 | 1926 | `				}` |
|         - | 1927 | `				/* COW separate before in-place mutation */` |
|         3 | 1928 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         2 | 1929 | `			}else{` |
|         - | 1930 | `				/* Create a new hashmap */` |
|      4471 | 1931 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      4471 | 1932 | `				if( pMap == 0){` |
|       ! 0 | 1933 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1934 | `					return SXERR_MEM;` |
|         - | 1935 | `				}` |
|         - | 1936 | `			}` |
|      4473 | 1937 | `			if( !bAddStore ){` |
|      4471 | 1938 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1939 | `					/* Perform a hashmap duplication */` |
|      4471 | 1940 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2238 | 1941 | `				}else{` |
|       ! 0 | 1942 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1943 | `						/* Simple insertion */` |
|       ! 0 | 1944 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 1945 | `					}` |
|         - | 1946 | `				}` |
|      2233 | 1947 | `			}` |
|         - | 1948 | `			/* Perform the union */` |
|      4473 | 1949 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      4473 | 1950 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2239 | 1951 | `			}else{` |
|       ! 0 | 1952 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1953 | `					/* Simple insertion */` |
|       ! 0 | 1954 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 1955 | `				}` |
|         - | 1956 | `			}` |
|         - | 1957 | `			/* Reflect the change */` |
|      4473 | 1958 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 1959 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 1960 | `			}` |
|      4473 | 1961 | `			pObj1->x.pOther = pMap;` |
|      4473 | 1962 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2234 | 1963 | `		}` |
|         - | 1964 | `	}` |
|     23979 | 1965 | `	return SXRET_OK;` |
|     11992 | 1966 | `}` |
|         - | 1967 | `/*` |
|         - | 1968 | ` * Return a printable representation of the type of a given` |
|         - | 1969 | ` * ph7_value.` |
|         - | 1970 | ` */` |
|       ! 0 | 1971 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       ! 0 | 1972 | `{` |
|       ! 0 | 1973 | `	const char *zType = "";` |
|       ! 0 | 1974 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 1975 | `		zType = "null";` |
|       ! 0 | 1976 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 1977 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 1978 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 1979 | `		zType = "double";` |
|       ! 0 | 1980 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 1981 | `		zType = "int";` |
|       ! 0 | 1982 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 1983 | `		zType = "string";` |
|       ! 0 | 1984 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 1985 | `		zType = "bool";` |
|       ! 0 | 1986 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       ! 0 | 1987 | `		zType = "array";` |
|       ! 0 | 1988 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 1989 | `		zType = "object";` |
|       ! 0 | 1990 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 1991 | `		zType = "resource";` |
|       ! 0 | 1992 | `	}` |
|       ! 0 | 1993 | `	return zType;` |
|       ! 0 | 1994 | `}` |
|         - | 1995 | `/*` |
|         - | 1996 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 1997 | ` * Store the dump in the given blob.` |
|         - | 1998 | ` */` |
|         - | 1999 | `/*` |
|         - | 2000 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2001 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2002 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2003 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2004 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2005 | ` */` |
|        76 | 2006 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         4 | 2007 | `{` |
|         - | 2008 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2009 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2010 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2011 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2012 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2013 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2014 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2015 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2016 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|        80 | 2017 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2018 | `#else` |
|         - | 2019 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2020 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2021 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2022 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2023 | `	}else{` |
|         - | 2024 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2025 | `	}` |
|         - | 2026 | `#endif` |
|        80 | 2027 | `}` |
|         - | 2028 | `/*` |
|         - | 2029 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2030 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2031 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2032 | ` */` |
|       254 | 2033 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         4 | 2034 | `{` |
|       258 | 2035 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|         7 | 2036 | `		return;` |
|         - | 2037 | `	}` |
|       252 | 2038 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2039 | `		if( pObj->x.iVal != 0 ){` |
|       ! 0 | 2040 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       ! 0 | 2041 | `		}` |
|       ! 0 | 2042 | `		return;` |
|         - | 2043 | `	}` |
|       252 | 2044 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2045 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2046 | `		 * non-strings into the output) */` |
|       141 | 2047 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       141 | 2048 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        69 | 2049 | `		}` |
|       141 | 2050 | `		return;` |
|         - | 2051 | `	}` |
|       114 | 2052 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       131 | 2053 | `}` |
|      3388 | 2054 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2055 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2056 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2057 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2058 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2059 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2060 | `	int nDepth,        /* Nesting level */` |
|         - | 2061 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2062 | `	)` |
|         5 | 2063 | `{` |
|      3393 | 2064 | `	sxi32 rc = SXRET_OK;` |
|         - | 2065 | `	int i;` |
|      3393 | 2066 | `	if( !ShowType ){` |
|         - | 2067 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2068 | `		 * containers render the Array/Object block (which the container` |
|         - | 2069 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       136 | 2070 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       123 | 2071 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2072 | `		}` |
|        15 | 2073 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        13 | 2074 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2075 | `		}` |
|         3 | 2076 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         3 | 2077 | `		return SXRET_OK;` |
|         - | 2078 | `	}` |
|         - | 2079 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2080 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2081 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|      8073 | 2082 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      4817 | 2083 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      2411 | 2084 | `	}` |
|      3261 | 2085 | `	if( isRef ){` |
|        22 | 2086 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        10 | 2087 | `	}` |
|      3261 | 2088 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       148 | 2089 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       148 | 2090 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2091 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|       ! 0 | 2092 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|       ! 0 | 2093 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|       ! 0 | 2094 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|       ! 0 | 2095 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|       ! 0 | 2096 | `			}` |
|       ! 0 | 2097 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       ! 0 | 2098 | `			return SXRET_OK;` |
|         - | 2099 | `		}` |
|       148 | 2100 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       148 | 2101 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       148 | 2102 | `		return rc;` |
|         - | 2103 | `	}` |
|      3117 | 2104 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        97 | 2105 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|        97 | 2106 | `		return SXRET_OK;` |
|         - | 2107 | `	}` |
|      3025 | 2108 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       225 | 2109 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       225 | 2110 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       225 | 2111 | `		return rc;` |
|         - | 2112 | `	}` |
|      2805 | 2113 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       885 | 2114 | `		if( pObj->x.iVal != 0 ){` |
|       559 | 2115 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       282 | 2116 | `		}else{` |
|       331 | 2117 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2118 | `		}` |
|       885 | 2119 | `		return SXRET_OK;` |
|         - | 2120 | `	}` |
|      1925 | 2121 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2122 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2123 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|        80 | 2124 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|        80 | 2125 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|        80 | 2126 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        80 | 2127 | `		return SXRET_OK;` |
|         - | 2128 | `	}` |
|      1849 | 2129 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|       965 | 2130 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|       965 | 2131 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       965 | 2132 | `		return SXRET_OK;` |
|         - | 2133 | `	}` |
|       889 | 2134 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       889 | 2135 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|       889 | 2136 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       789 | 2137 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       392 | 2138 | `		}` |
|       889 | 2139 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|       889 | 2140 | `		return SXRET_OK;` |
|         - | 2141 | `	}` |
|       ! 0 | 2142 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2143 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2144 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2145 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2146 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2147 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2148 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2149 | `		return SXRET_OK;` |
|         - | 2150 | `	}` |
|         - | 2151 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2152 | `	{` |
|       ! 0 | 2153 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2154 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2155 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2156 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2157 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2158 | `	}` |
|       ! 0 | 2159 | `	return rc;` |
|      1699 | 2160 | `}` |
|         - | 2161 |  |
