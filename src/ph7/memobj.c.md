# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 959/1131 lines (84.79%)

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
|         5 |   38 | `		return 0;` |
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
|      1256 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|      1261 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      1143 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|         - |   69 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|         - |   70 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|         - |   71 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|         - |   72 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|         - |   73 | `	 * A value that IS a float is a float whatever it has cached. */` |
|      1133 |   74 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      1077 |   75 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|       721 |   76 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       219 |   77 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        36 |   78 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        36 |   79 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   80 | `	return "unknown";` |
|       633 |   81 | `}` |
|         - |   82 |  |
|         - |   83 | `/*` |
|         - |   84 | ` * Notes on memory objects [i.e: ph7_value].` |
|         - |   85 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |   86 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|         - |   87 | ` * Each ph7_values struct may cache multiple representations (string,` |
|         - |   88 | ` * integer etc.) of the same value.` |
|         - |   89 | ` */` |
|         - |   90 | `/*` |
|         - |   91 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer.` |
|         - |   92 | ` * If the double is too large, return 0x8000000000000000.` |
|         - |   93 | ` *` |
|         - |   94 | ` * Most systems appear to do this simply by assigning ariables and without` |
|         - |   95 | ` * the extra range tests.` |
|         - |   96 | ` * But there are reports that windows throws an expection if the floating` |
|         - |   97 | ` * point value is out of range.` |
|         - |   98 | ` */` |
|     14354 |   99 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |  100 | `{` |
|         - |  101 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  102 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |  103 | `	 * is omitted from the build.` |
|         - |  104 | `	 */` |
|         - |  105 | `	return pObj->rVal;` |
|         - |  106 | `#else` |
|         - |  107 | ` /*` |
|         - |  108 | `  ** Many compilers we encounter do not define constants for the` |
|         - |  109 | `  ** minimum and maximum 64-bit integers, or they define them` |
|         - |  110 | `  ** inconsistently.  And many do not understand the "LL" notation.` |
|         - |  111 | `  ** So we define our own static constants here using nothing` |
|         - |  112 | `  ** larger than a 32-bit integer constant.` |
|         - |  113 | `  */` |
|         - |  114 | `  static const sxi64 minInt = SMALLEST_INT64;` |
|     14359 |  115 | `  ph7_real r = pObj->rVal;` |
|         - |  116 | `  /* The bounds are tested in DOUBLE space, and the arithmetic there is exact:` |
|         - |  117 | `  ** (ph7_real)minInt is -2^63 to the bit, so -(ph7_real)minInt is +2^63 -- one` |
|         - |  118 | `  ** past the range -- and no double exists between LARGEST_INT64 and it. Hence` |
|         - |  119 | ``  ** `>=` on the way up and `<` on the way down.`` |
|         - |  120 | `  **` |
|         - |  121 | ``  ** The upper test used to be `r > (ph7_real)maxInt`, and (ph7_real)maxInt ROUNDS`` |
|         - |  122 | ``  ** UP to 2^63: a double of exactly 2^63 passed the guard and reached `(sxi64)r`,`` |
|         - |  123 | `  ** which is undefined behaviour. x86 happens to answer minInt there -- the same` |
|         - |  124 | `  ** value this returns -- but aarch64 saturates to maxInt, so PHL answered two` |
|         - |  125 | ``  ** different values for `(int)9.2233720368547758E+18` on the two platforms it`` |
|         - |  126 | `  ** builds for. NaN compares false against every bound and reached the same cast,` |
|         - |  127 | `  ** so it is screened here too.` |
|         - |  128 | `  **` |
|         - |  129 | `  ** minInt is deliberate for BOTH directions, not maxInt going up: it is the` |
|         - |  130 | `  ** answer x86's cast produced, and the corpus pins it. php's own answer for a` |
|         - |  131 | `  ** non-representable float is a modular wrap (and a warning) -- a divergence` |
|         - |  132 | `  ** recorded in §2, not something this boundary fix changes. */` |
|     14359 |  133 | `  if( PH7_IS_NAN(r) \|\| r < (ph7_real)minInt \|\| r >= -(ph7_real)minInt ){` |
|       864 |  134 | `    return minInt;` |
|       ! 0 |  135 | `  }else{` |
|     13499 |  136 | `    return (sxi64)r;` |
|         - |  137 | `  }` |
|         - |  138 | `#endif` |
|      7182 |  139 | `}` |
|         - |  140 | `/*` |
|         - |  141 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  142 | ` * to a 64-bit integer.` |
|         - |  143 | ` */` |
|    646650 |  144 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  145 | `{` |
|    646655 |  146 | `	sxi64 iVal = 0;` |
|    646655 |  147 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  148 | `		return 0;` |
|         - |  149 | `	}` |
|    646655 |  150 | `	if( pVal->zString[0] == '0' ){` |
|         - |  151 | `		sxi32 c;` |
|    228047 |  152 | `		if( pVal->nByte == sizeof(char) ){` |
|    222903 |  153 | `			return 0;` |
|         - |  154 | `		}` |
|      5149 |  155 | `		c = pVal->zString[1];` |
|      5149 |  156 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  157 | `			/* Hex digit stream */` |
|        96 |  158 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      5102 |  159 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  160 | `			/* Binary digit stream */` |
|       285 |  161 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      4913 |  162 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  163 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  164 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  165 | `			if( pVal->nByte > 2 ){` |
|        21 |  166 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  167 | `			}` |
|        11 |  168 | `		}else{` |
|         - |  169 | `			/* Legacy octal digit stream (leading 0) */` |
|      4751 |  170 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  171 | `		}` |
|      2577 |  172 | `	}else{` |
|         - |  173 | `		/* Decimal digit stream */` |
|    418613 |  174 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  175 | `	}` |
|    423757 |  176 | `	return iVal;` |
|    323330 |  177 | `}` |
|         - |  178 | `/*` |
|         - |  179 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  180 | ` * do at representing the value that pObj describes as a string` |
|         - |  181 | ` * representation.` |
|         - |  182 | ` */` |
|      2222 |  183 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  184 | `{` |
|      2227 |  185 | `	sxi64 iVal = 0;` |
|         - |  186 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  187 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  188 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      2227 |  189 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|      2227 |  190 | `	return iVal;` |
|         5 |  191 | `}` |
|         - |  192 | `/*` |
|         - |  193 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  194 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  195 | ` * successfully called. Any other return value indicates failure.` |
|         - |  196 | ` */` |
|       880 |  197 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  198 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  199 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  200 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  201 | `	sxu32 nLen,                /* Method name length */` |
|         - |  202 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  203 | `	)` |
|         5 |  204 | `{` |
|         - |  205 | `	ph7_class_method *pMethod;` |
|         - |  206 | `	/* Check if the method is available */` |
|       885 |  207 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       885 |  208 | `	if( pMethod == 0 ){` |
|         - |  209 | `		/* No such method */` |
|         6 |  210 | `		return SXERR_NOTFOUND;` |
|         - |  211 | `	}` |
|         - |  212 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  213 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  214 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  215 | `	 * printed "Object" after a caught __toString() throw). */` |
|       881 |  216 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       445 |  217 | `}` |
|         - |  218 | `/*` |
|         - |  219 | ` * Return some kind of integer value which is the best we can` |
|         - |  220 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  221 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  222 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  223 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  224 | ` * a integer and return that.` |
|         - |  225 | ` * If pObj represents a NULL value, return 0.` |
|         - |  226 | ` */` |
|      1670 |  227 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  228 | `{` |
|         - |  229 | `	sxi32 iFlags;` |
|      1675 |  230 | `	iFlags = pObj->iFlags;` |
|      1675 |  231 | `	if (iFlags & MEMOBJ_REAL ){` |
|        93 |  232 | `		return MemObjRealToInt(&(*pObj));` |
|      1585 |  233 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       421 |  234 | `		return pObj->x.iVal;` |
|      1169 |  235 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  236 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  237 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  238 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|      1121 |  239 | `		return MemObjStringToInt(&(*pObj),0);` |
|        52 |  240 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        31 |  241 | `		return 0;` |
|        22 |  242 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  243 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  244 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  245 | `		 * followed php; int/float did not.) */` |
|         7 |  246 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|         7 |  247 | `		sxu32 n = pMap->nEntry;` |
|         7 |  248 | `		PH7_HashmapUnref(pMap);` |
|         7 |  249 | `		return n > 0 ? 1 : 0;` |
|        16 |  250 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  251 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  252 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  253 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|         7 |  254 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         7 |  255 | `		if( pInst && pInst->pClass ){` |
|        10 |  256 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         6 |  257 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|         3 |  258 | `		}` |
|         7 |  259 | `		PH7_ClassInstanceUnref(pInst);` |
|         7 |  260 | `		return 1;` |
|        10 |  261 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  262 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  263 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        10 |  264 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  265 | `	}` |
|         - |  266 | `	/* CANT HAPPEN */` |
|       ! 0 |  267 | `	return 0;` |
|       840 |  268 | `}` |
|         - |  269 | `/*` |
|         - |  270 | ` * Return some kind of real value which is the best we can` |
|         - |  271 | ` * do at representing the value that pObj describes as a real.` |
|         - |  272 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  273 | ` * integer then the integer  is promoted to real and that value` |
|         - |  274 | ` * is returned.` |
|         - |  275 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  276 | ` * into a real and return that.` |
|         - |  277 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  278 | ` */` |
|     12876 |  279 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  280 | `{` |
|         - |  281 | `	sxi32 iFlags;` |
|     12881 |  282 | `	iFlags = pObj->iFlags;` |
|     12881 |  283 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  284 | `		return pObj->rVal;` |
|     12881 |  285 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      1337 |  286 | `		return (ph7_real)pObj->x.iVal;` |
|     11549 |  287 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  288 | `		SyString sString;` |
|         - |  289 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  290 | `		ph7_real rVal = 0;` |
|         - |  291 | `#else` |
|     11529 |  292 | `		ph7_real rVal = 0.0;` |
|         - |  293 | `#endif` |
|     11529 |  294 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     11529 |  295 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  296 | `			/* Convert as much as we can */` |
|         - |  297 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  298 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  299 | `#else` |
|     11525 |  300 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  301 | `#endif` |
|      5760 |  302 | `		}` |
|     11529 |  303 | `		return rVal;` |
|        22 |  304 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  305 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  306 | `		return 0;` |
|         - |  307 | `#else` |
|         9 |  308 | `		return 0.0;` |
|         - |  309 | `#endif` |
|        14 |  310 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  311 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  312 | `		 * branch above. */` |
|       ! 0 |  313 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  314 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  315 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  316 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        14 |  317 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  318 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        12 |  319 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        12 |  320 | `		if( pInst && pInst->pClass ){` |
|        17 |  321 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        10 |  322 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|         5 |  323 | `		}` |
|        12 |  324 | `		PH7_ClassInstanceUnref(pInst);` |
|        12 |  325 | `		return (ph7_real)1.0;` |
|         3 |  326 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  327 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  328 | `	}` |
|         - |  329 | `	/* NOT REACHED  */` |
|       ! 0 |  330 | `	return 0;` |
|      6443 |  331 | `}` |
|         - |  332 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  333 | `/*` |
|         - |  334 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  335 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  336 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  337 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  338 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  339 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  340 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  341 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  342 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  343 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  344 | ` */` |
|       626 |  345 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  346 | `{` |
|         - |  347 | `	sxi32 iExp,i;` |
|       631 |  348 | `	iExp = nLen - 1;` |
|      4805 |  349 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      4179 |  350 | `		iExp--;` |
|         5 |  351 | `	}` |
|       631 |  352 | `	if( iExp <= 0 ){` |
|       577 |  353 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  354 | `	}` |
|         - |  355 | `	{` |
|        56 |  356 | `		sxi32 iDig = iExp + 1;` |
|         - |  357 | `		sxi32 iFirst;` |
|        56 |  358 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|        56 |  359 | `			iDig++;` |
|        27 |  360 | `		}` |
|        56 |  361 | `		iFirst = iDig;` |
|        94 |  362 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|        69 |  363 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        27 |  364 | `			iFirst++;` |
|         1 |  365 | `		}` |
|        56 |  366 | `		if( iFirst > iDig ){` |
|        27 |  367 | `			sxi32 nStrip = iFirst - iDig;` |
|        79 |  368 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        53 |  369 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        27 |  370 | `			}` |
|        27 |  371 | `			nLen -= nStrip;` |
|        13 |  372 | `		}` |
|         - |  373 | `	}` |
|        56 |  374 | `	if( bGeneric ){` |
|        40 |  375 | `		int bHasDot = 0;` |
|        80 |  376 | `		for( i = 0 ; i < iExp ; i++ ){` |
|        54 |  377 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        22 |  378 | `		}` |
|        40 |  379 | `		if( !bHasDot ){` |
|       154 |  380 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       128 |  381 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        65 |  382 | `			}` |
|        28 |  383 | `			zBuf[iExp] = '.';` |
|        28 |  384 | `			zBuf[iExp+1] = '0';` |
|        28 |  385 | `			nLen += 2;` |
|        13 |  386 | `		}` |
|        19 |  387 | `	}` |
|        56 |  388 | `	return nLen;` |
|       318 |  389 | `}` |
|         - |  390 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  391 | `/*` |
|         - |  392 | ` * Return the string representation of a given ph7_value.` |
|         - |  393 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  394 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  395 | ` * left without a rendering of pObj.` |
|         - |  396 | ` */` |
|     70770 |  397 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  398 | `{` |
|     70775 |  399 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  400 | `		/* Handle special floating-point values first */` |
|       481 |  401 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|       ! 0 |  402 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       481 |  403 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|         5 |  404 | `			if( pObj->rVal < 0.0 ){` |
|       ! 0 |  405 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|       ! 0 |  406 | `			}else{` |
|         5 |  407 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  408 | `			}` |
|         3 |  409 | `		}else{` |
|         - |  410 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  411 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  412 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  413 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  414 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  415 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  416 | `			 * exponent/fraction quirks. */` |
|         - |  417 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       477 |  418 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       477 |  419 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  420 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  421 | `			}` |
|       477 |  422 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       477 |  423 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  424 | `#else` |
|         - |  425 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  426 | `#endif` |
|         5 |  427 | `		}` |
|     70537 |  428 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     69039 |  429 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  430 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     35782 |  431 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       119 |  432 | `		if( bStrictBool ){` |
|         - |  433 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       119 |  434 | `			if( pObj->x.iVal ){` |
|        77 |  435 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        36 |  436 | `			}` |
|         - |  437 | `			/* false produces empty string, nothing to append */` |
|        62 |  438 | `		}else{` |
|         - |  439 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  440 | `			if( pObj->x.iVal ){` |
|       ! 0 |  441 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  442 | `			}else{` |
|       ! 0 |  443 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  444 | `			}` |
|         5 |  445 | `		}` |
|      1208 |  446 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       115 |  447 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       115 |  448 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      1095 |  449 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  450 | `		ph7_value sResult;` |
|         - |  451 | `		sxi32 rc;` |
|         - |  452 | `		/* Invoke the __toString() method if available */` |
|       885 |  453 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       885 |  454 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  455 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       885 |  456 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  457 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  458 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  459 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  460 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  461 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  462 | `			 * instance reference. */` |
|       147 |  463 | `			PH7_MemObjRelease(&sResult);` |
|       147 |  464 | `			return rc;` |
|         - |  465 | `		}` |
|       743 |  466 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  467 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  468 | `			 * value, and requiring a non-empty one sent` |
|         - |  469 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  470 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  471 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  472 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  473 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  474 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  475 | `			 * result no return-type check governs. */` |
|       739 |  476 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       372 |  477 | `		}else{` |
|         - |  478 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  479 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         6 |  480 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  481 | `		}` |
|       743 |  482 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       743 |  483 | `		PH7_MemObjRelease(&sResult);` |
|       528 |  484 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  485 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  486 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  487 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  488 | `	}` |
|     70633 |  489 | `	return SXRET_OK;` |
|     35390 |  490 | `}` |
|         - |  491 | `/*` |
|         - |  492 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  493 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  494 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  495 | ` * (php's exact set):` |
|         - |  496 | ` * NULL` |
|         - |  497 | ` * the boolean FALSE itself.` |
|         - |  498 | ` * the integer 0 (zero).` |
|         - |  499 | ` * the real 0.0 (zero).` |
|         - |  500 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  501 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  502 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  503 | ` * and were removed under the §10 PH7-ism policy).` |
|         - |  504 | ` * an array with zero elements.` |
|         - |  505 | ` */` |
|     61014 |  506 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  507 | `{` |
|         - |  508 | `	sxi32 iFlags;` |
|     61019 |  509 | `	iFlags = pObj->iFlags;` |
|     61019 |  510 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  511 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  512 | `		return pObj->rVal ? 1 : 0;` |
|         - |  513 | `#else` |
|        16 |  514 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  515 | `#endif` |
|     61005 |  516 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       525 |  517 | `		return pObj->x.iVal ? 1 : 0;` |
|     60485 |  518 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  519 | `		SyString sString;` |
|        97 |  520 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  521 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|        97 |  522 | `		if( sString.nByte == 0 ){` |
|        19 |  523 | `			return 0;` |
|         - |  524 | `		}` |
|        81 |  525 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        10 |  526 | `			return 0;` |
|         - |  527 | `		}` |
|        73 |  528 | `		return 1;` |
|     60391 |  529 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     59077 |  530 | `		return 0;` |
|      1319 |  531 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        42 |  532 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        42 |  533 | `		sxu32 n = pMap->nEntry;` |
|        42 |  534 | `		PH7_HashmapUnref(pMap);` |
|        42 |  535 | `		return n > 0 ? TRUE : FALSE;` |
|      1279 |  536 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  537 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  538 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  539 | `		 * extension changed control flow in valid php source. */` |
|        38 |  540 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        38 |  541 | `		return 1;` |
|      1243 |  542 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1243 |  543 | `		return pObj->x.pOther != 0;` |
|         - |  544 | `	}` |
|         - |  545 | `	/* NOT REACHED */` |
|       ! 0 |  546 | `	return 0;` |
|     30512 |  547 | `}` |
|         - |  548 | `/*` |
|         - |  549 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  550 | ` */` |
|     14264 |  551 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  552 | `{` |
|     14269 |  553 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  554 | `  /* Only mark the value as an integer if` |
|         - |  555 | `  **` |
|         - |  556 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  557 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  558 | `  **        possible integer` |
|         - |  559 | `  **` |
|         - |  560 | `  ** The second and third terms in the following conditional enforces` |
|         - |  561 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  562 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  563 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  564 | `  ** architectures might behave differently.` |
|         - |  565 | `  */` |
|     14264 |  566 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     11898 |  567 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     11873 |  568 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      5936 |  569 | `	}` |
|     14269 |  570 | `	return SXRET_OK;` |
|         5 |  571 | `}` |
|         - |  572 | `/*` |
|         - |  573 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  574 | ` */` |
|    750848 |  575 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  576 | `{` |
|    750853 |  577 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  578 | `		/* Preform the conversion */` |
|      1675 |  579 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  580 | `		/* Invalidate any prior representations */` |
|      1675 |  581 | `		SyBlobRelease(&pObj->sBlob);` |
|      1675 |  582 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|       835 |  583 | `	}` |
|    750853 |  584 | `	return SXRET_OK;` |
|         5 |  585 | `}` |
|         - |  586 | `/*` |
|         - |  587 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  588 | ` * Invalidate any prior representations` |
|         - |  589 | ` */` |
|     14110 |  590 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  591 | `{` |
|     14115 |  592 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  593 | `		/* Preform the conversion */` |
|     12881 |  594 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  595 | `		/* Invalidate any prior representations */` |
|     12881 |  596 | `		SyBlobRelease(&pObj->sBlob);` |
|     12881 |  597 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  598 | `		/* Try to get an integer representation */` |
|     12881 |  599 | `		MemObjTryIntger(&(*pObj));` |
|      6438 |  600 | `	}` |
|     14115 |  601 | `	return SXRET_OK;` |
|         5 |  602 | `}` |
|         - |  603 | `/*` |
|         - |  604 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  605 | ` */` |
|     73000 |  606 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  607 | `{` |
|     73005 |  608 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  609 | `		/* Preform the conversion */` |
|     61019 |  610 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  611 | `		/* Invalidate any prior representations */` |
|     61019 |  612 | `		SyBlobRelease(&pObj->sBlob);` |
|     61019 |  613 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     30507 |  614 | `	}` |
|     73005 |  615 | `	return SXRET_OK;` |
|         5 |  616 | `}` |
|         - |  617 | `/*` |
|         - |  618 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  619 | ` */` |
|   4176729 |  620 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  621 | `{` |
|   4176734 |  622 | `	sxi32 rc = SXRET_OK;` |
|   4176734 |  623 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  624 | `		/* Perform the conversion */` |
|     70629 |  625 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     70629 |  626 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     70629 |  627 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  628 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  629 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  630 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  631 | `			 * leaves $o holding the object after the throw is caught. */` |
|       147 |  632 | `			return rc;` |
|         - |  633 | `		}` |
|     70487 |  634 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     35241 |  635 | `	}` |
|   4176592 |  636 | `	return rc;` |
|   2090186 |  637 | `}` |
|         - |  638 | `/*` |
|         - |  639 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  640 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  641 | ` *   Error: Object of class X could not be converted to string` |
|         - |  642 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  643 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  644 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  645 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  646 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  647 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  648 | ` * cast still carried the placeholder.` |
|         - |  649 | ` *` |
|         - |  650 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  651 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - |  652 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - |  653 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - |  654 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - |  655 | ` * settype() site then blanks its target itself, because php's` |
|         - |  656 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - |  657 | ` */` |
|       546 |  658 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         3 |  659 | `{` |
|       549 |  660 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - |  661 | `	SyBlob sMsg;` |
|       549 |  662 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       549 |  663 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       546 |  664 | `		&pInst->pClass->sName);` |
|         - |  665 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       549 |  666 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         3 |  667 | `}` |
|         - |  668 | `/*` |
|         - |  669 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - |  670 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - |  671 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - |  672 | ` */` |
|     72076 |  673 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 |  674 | `{` |
|         - |  675 | `	ph7_class_instance *pInst;` |
|     72081 |  676 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|     70619 |  677 | `		return FALSE;` |
|         - |  678 | `	}` |
|      1467 |  679 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      1467 |  680 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 |  681 | `		return FALSE;` |
|         - |  682 | `	}` |
|      1467 |  683 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     36043 |  684 | `}` |
|         - |  685 | `/*` |
|         - |  686 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  687 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  688 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  689 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  690 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  691 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  692 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  693 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  694 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  695 | ` *` |
|         - |  696 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  697 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - |  698 | ` * ph7_value carries.` |
|         - |  699 | ` *` |
|         - |  700 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - |  701 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - |  702 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - |  703 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - |  704 | ` * exactly as php never throws for them.` |
|         - |  705 | ` *` |
|         - |  706 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - |  707 | ` */` |
|    820056 |  708 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  709 | `{` |
|    820061 |  710 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|    750899 |  711 | `		return SXRET_OK;` |
|         - |  712 | `	}` |
|     69167 |  713 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|        85 |  714 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        41 |  715 | `	}` |
|     69167 |  716 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       549 |  717 | `		return MemObjThrowNotStringable(pObj);` |
|         - |  718 | `	}` |
|     68621 |  719 | `	return PH7_MemObjToString(pObj);` |
|    410033 |  720 | `}` |
|         - |  721 | `/*` |
|         - |  722 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  723 | ` * representation.` |
|         - |  724 | ` */` |
|         2 |  725 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  726 | `{` |
|         3 |  727 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  728 | `}` |
|         - |  729 | `/*` |
|         - |  730 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  731 | `  * According to the PHP language reference manual.` |
|         - |  732 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  733 | `  *   to an array results in an array with a single element with index zero` |
|         - |  734 | `  *   and the value of the scalar which was converted.` |
|         - |  735 | `  */` |
|      2140 |  736 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  737 | `{` |
|      2145 |  738 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  739 | `		ph7_hashmap *pMap;` |
|         - |  740 | `		/* Allocate a new hashmap instance */` |
|      1883 |  741 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      1883 |  742 | `		if( pMap == 0 ){` |
|       ! 0 |  743 | `			return SXERR_MEM;` |
|         - |  744 | `		}` |
|      1883 |  745 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  746 | `			/*` |
|         - |  747 | `			 * According to the PHP language reference manual.` |
|         - |  748 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  749 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  750 | `			 *   and the value of the scalar which was converted.` |
|         - |  751 | `			 */` |
|       141 |  752 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       115 |  753 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       110 |  754 | `				if( pInst && pObj->pVm->pClosureClass` |
|       115 |  755 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|         - |  756 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|         - |  757 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|         - |  758 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|         - |  759 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|         - |  760 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|         3 |  761 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         2 |  762 | `				}else{` |
|         - |  763 | `					/* Object cast */` |
|       112 |  764 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|         - |  765 | `				}` |
|        60 |  766 | `			}else{` |
|         - |  767 | `				/* Insert a single element */` |
|        28 |  768 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  769 | `			}` |
|       141 |  770 | `			SyBlobRelease(&pObj->sBlob);` |
|        68 |  771 | `		}` |
|         - |  772 | `		/* Invalidate any prior representation */` |
|      1883 |  773 | `		PH7_MemObjRelease(pObj);` |
|      1883 |  774 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      1883 |  775 | `		pObj->x.pOther = pMap;` |
|       939 |  776 | `	}` |
|      2145 |  777 | `	return SXRET_OK;` |
|      1075 |  778 | `}` |
|         - |  779 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  780 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  781 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  782 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|        90 |  783 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         4 |  784 | `{` |
|        94 |  785 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  786 | `	ph7_value *pSlot;` |
|         - |  787 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  788 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  789 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  790 | `	 * safe to coerce in place. */` |
|        94 |  791 | `	PH7_MemObjToString(pKey);` |
|       139 |  792 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|        90 |  793 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|        94 |  794 | `	if( pSlot ){` |
|        94 |  795 | `		PH7_MemObjStore(pValue,pSlot);` |
|        45 |  796 | `	}` |
|        94 |  797 | `	return SXRET_OK;` |
|         4 |  798 | `}` |
|         - |  799 | `/*` |
|         - |  800 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  801 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  802 | ` * matching PHP's (object) cast:` |
|         - |  803 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  804 | ` *   - scalar -> a single property named "scalar".` |
|         - |  805 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  806 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  807 | ` */` |
|        72 |  808 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         4 |  809 | `{` |
|        76 |  810 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  811 | `		ph7_class_instance *pStd;` |
|         - |  812 | `		ph7_class *pClass;` |
|         - |  813 | `		ph7_vm *pVm;` |
|         - |  814 | `		/* Point to the underlying VM + the stdClass */` |
|        76 |  815 | `		pVm = pObj->pVm;` |
|       112 |  816 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        36 |  817 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        76 |  818 | `		if( pClass == 0 ){` |
|         - |  819 | `			/* Can't happen,load null instead */` |
|       ! 0 |  820 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  821 | `			return SXRET_OK;` |
|         - |  822 | `		}` |
|         - |  823 | `		/* Instanciate a new (empty) stdClass object */` |
|        76 |  824 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|        76 |  825 | `		if( pStd == 0 ){` |
|         - |  826 | `			/* Out of memory */` |
|       ! 0 |  827 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  828 | `			return SXRET_OK;` |
|         - |  829 | `		}` |
|        76 |  830 | `		pStd->iRef = 1;` |
|        76 |  831 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - |  832 | `			/* Array: one dynamic property per entry. */` |
|         - |  833 | `			struct VmObjCastData sData;` |
|        62 |  834 | `			sData.pVm = pVm;` |
|        62 |  835 | `			sData.pStd = pStd;` |
|        62 |  836 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        45 |  837 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - |  838 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 |  839 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 |  840 | `			if( pSlot ){` |
|        14 |  841 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 |  842 | `			}` |
|         6 |  843 | `		}` |
|         - |  844 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - |  845 | `		/* Invalidate any prior representation */` |
|        76 |  846 | `		PH7_MemObjRelease(pObj);` |
|         - |  847 | `		/* Save the new instance */` |
|        76 |  848 | `		pObj->x.pOther = pStd;` |
|        76 |  849 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        36 |  850 | `	}` |
|        76 |  851 | `	return SXRET_OK;` |
|        40 |  852 | `}` |
|         - |  853 | `/*` |
|         - |  854 | ` * Return a pointer to the appropriate convertion method associated` |
|         - |  855 | ` * with the given type.` |
|         - |  856 | ` * Note on type juggling.` |
|         - |  857 | ` * Accoding to the PHP language reference manual` |
|         - |  858 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - |  859 | ` *  declaration; a variable's type is determined by the context in which` |
|         - |  860 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - |  861 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - |  862 | ` *  assigned to $var, it becomes an integer.` |
|         - |  863 | ` */` |
|    100248 |  864 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 |  865 | `{` |
|    100253 |  866 | `	if( iFlags & MEMOBJ_STRING ){` |
|        98 |  867 | `		return PH7_MemObjToString;` |
|    100159 |  868 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100103 |  869 | `		return PH7_MemObjToInteger;` |
|        61 |  870 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        52 |  871 | `		return PH7_MemObjToReal;` |
|        11 |  872 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 |  873 | `		return PH7_MemObjToBool;` |
|         8 |  874 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 |  875 | `		return PH7_MemObjToHashmap;` |
|       ! 0 |  876 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  877 | `		return PH7_MemObjToObject;` |
|       ! 0 |  878 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  879 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - |  880 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - |  881 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - |  882 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - |  883 | `		 * default. */` |
|       ! 0 |  884 | `		return 0;` |
|         - |  885 | `	}` |
|         - |  886 | `	/* NULL cast */` |
|       ! 0 |  887 | `	return PH7_MemObjToNull;` |
|     50129 |  888 | `}` |
|         - |  889 | `/*` |
|         - |  890 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - |  891 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - |  892 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - |  893 | ` * loose-comparison numeric gate:` |
|         - |  894 | ` *` |
|         - |  895 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - |  896 | ` *` |
|         - |  897 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - |  898 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - |  899 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - |  900 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - |  901 | ` * a non-string value.` |
|         - |  902 | ` */` |
|         - |  903 | `/*` |
|         - |  904 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - |  905 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - |  906 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - |  907 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - |  908 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - |  909 | ` * and rejects a string with no prefix outright.` |
|         - |  910 | ` */` |
|    392457 |  911 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 |  912 | `{` |
|         - |  913 | `	const char *z, *zEnd;` |
|         - |  914 | `	sxu32 n;` |
|    392462 |  915 | `	int bDigit = 0;` |
|    392462 |  916 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  917 | `		return 0;` |
|         - |  918 | `	}` |
|    392462 |  919 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|    392462 |  920 | `	n = SyBlobLength(&pValue->sBlob);` |
|    392462 |  921 | `	if( n == 0 ){` |
|       124 |  922 | `		return 0;` |
|         - |  923 | `	}` |
|    392342 |  924 | `	zEnd = z + n;` |
|    392578 |  925 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       239 |  926 | `		z++;` |
|         3 |  927 | `	}` |
|    392342 |  928 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       511 |  929 | `		z++;` |
|       253 |  930 | `	}` |
|    460978 |  931 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     68641 |  932 | `		z++; bDigit = 1;` |
|         5 |  933 | `	}` |
|    392342 |  934 | `	if( z < zEnd && z[0] == '.' ){` |
|      5477 |  935 | `		z++;` |
|      6589 |  936 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1116 |  937 | `			z++; bDigit = 1;` |
|         4 |  938 | `		}` |
|      2894 |  939 | `	}` |
|         - |  940 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|    392342 |  941 | `	if( !bDigit ){` |
|    388946 |  942 | `		return 0;` |
|         - |  943 | `	}` |
|         - |  944 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - |  945 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|      3401 |  946 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       172 |  947 | `		const char *zExp = z;` |
|       172 |  948 | `		z++;` |
|       172 |  949 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         7 |  950 | `			z++;` |
|         3 |  951 | `		}` |
|       172 |  952 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        10 |  953 | `			z = zExp;` |
|         6 |  954 | `		}else{` |
|       366 |  955 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       206 |  956 | `				z++;` |
|         4 |  957 | `			}` |
|         - |  958 | `		}` |
|        84 |  959 | `	}` |
|      3401 |  960 | `	if( pzTail ){` |
|      3401 |  961 | `		*pzTail = z;` |
|      1698 |  962 | `	}` |
|      3401 |  963 | `	return 1;` |
|    196083 |  964 | `}` |
|         - |  965 | `/*` |
|         - |  966 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - |  967 | ` * (trailing whitespace allowed, nothing else).` |
|         - |  968 | ` */` |
|    390767 |  969 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 |  970 | `{` |
|    390772 |  971 | `	const char *zTail = 0, *zEnd;` |
|    390772 |  972 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    389004 |  973 | `		return 0;` |
|         - |  974 | `	}` |
|      1773 |  975 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      1817 |  976 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        47 |  977 | `		zTail++;` |
|         3 |  978 | `	}` |
|      1773 |  979 | `	return zTail == zEnd ? 1 : 0;` |
|    195238 |  980 | `}` |
|         - |  981 | `/*` |
|         - |  982 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - |  983 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - |  984 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - |  985 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - |  986 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - |  987 | ` * those bytes convert to when it did.` |
|         - |  988 | ` *` |
|         - |  989 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - |  990 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - |  991 | ` * however large it is, because it was always going to be a double, so making it` |
|         - |  992 | ` * one lost no digits.` |
|         - |  993 | ` *` |
|         - |  994 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - |  995 | ` * intact when this says no.` |
|         - |  996 | ` */` |
|       668 |  997 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         2 |  998 | `{` |
|       670 |  999 | `	const char *z, *zTail = 0;` |
|       670 | 1000 | `	int iOverflow = 0;` |
|       670 | 1001 | `	*piOverflow = 0;` |
|       670 | 1002 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|       104 | 1003 | `		return FALSE;` |
|         - | 1004 | `	}` |
|       568 | 1005 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 | 1006 | `		return FALSE;` |
|         - | 1007 | `	}` |
|         - | 1008 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - | 1009 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|       568 | 1010 | `	z = (const char *)SyBlobData(&pObj->sBlob);` |
|     20368 | 1011 | `	while( z < zTail ){` |
|     19882 | 1012 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|        82 | 1013 | `			return FALSE;` |
|         - | 1014 | `		}` |
|     19802 | 1015 | `		z++;` |
|         2 | 1016 | `	}` |
|       488 | 1017 | `	MemObjStringToInt(pObj,&iOverflow);` |
|       488 | 1018 | `	*piOverflow = iOverflow;` |
|       488 | 1019 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1020 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1021 | `	}` |
|       488 | 1022 | `	return TRUE;` |
|       336 | 1023 | `}` |
|         - | 1024 | `/*` |
|         - | 1025 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1026 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1027 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1028 | ` */` |
|    295391 | 1029 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1030 | `{` |
|    295396 | 1031 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      1479 | 1032 | `		return TRUE;` |
|    293922 | 1033 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      4277 | 1034 | `		return FALSE;` |
|    289650 | 1035 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1036 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    289650 | 1037 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1038 | `	}` |
|         - | 1039 | `	/* NOT REACHED */` |
|       ! 0 | 1040 | `	return FALSE;` |
|    147550 | 1041 | `}` |
|         - | 1042 | `/*` |
|         - | 1043 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1044 | ` * FALSE otherwise.` |
|         - | 1045 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1046 | ` * NULL value.` |
|         - | 1047 | ` * Boolean FALSE.` |
|         - | 1048 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1049 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1050 | ` * An empty array.` |
|         - | 1051 | ` * NOTE` |
|         - | 1052 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1053 | ` */` |
|     45768 | 1054 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1055 | `{` |
|     45773 | 1056 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        42 | 1057 | `		return TRUE;` |
|     45735 | 1058 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|        22 | 1059 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|     45715 | 1060 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 1061 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|     45715 | 1062 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|         5 | 1063 | `		return !pObj->x.iVal;` |
|     45711 | 1064 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     30327 | 1065 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|     23989 | 1066 | `			return TRUE;` |
|       ! 0 | 1067 | `		}else{` |
|         - | 1068 | `			const char *zIn,*zEnd;` |
|      6343 | 1069 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|      6343 | 1070 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|      6351 | 1071 | `			while( zIn < zEnd ){` |
|      6351 | 1072 | `				if( zIn[0] != '0' ){` |
|      6343 | 1073 | `					break;` |
|         - | 1074 | `				}` |
|        10 | 1075 | `				zIn++;` |
|         2 | 1076 | `			}` |
|      6343 | 1077 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|       ! 0 | 1078 | `		}` |
|     15389 | 1079 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     15389 | 1080 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     15389 | 1081 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|       ! 0 | 1082 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       ! 0 | 1083 | `		return FALSE;` |
|         - | 1084 | `	}` |
|         - | 1085 | `	/* Assume empty by default */` |
|       ! 0 | 1086 | `	return TRUE;` |
|     22889 | 1087 | `}` |
|         - | 1088 | `/*` |
|         - | 1089 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1090 | ` * or both.` |
|         - | 1091 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1092 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1093 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1094 | ` * and ignore the rest.` |
|         - | 1095 | ` */` |
|    896889 | 1096 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1097 | `{` |
|    896894 | 1098 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|    896126 | 1099 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        60 | 1100 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        41 | 1101 | `				pObj->x.iVal = 0;` |
|        19 | 1102 | `			}` |
|        60 | 1103 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        28 | 1104 | `		}` |
|         - | 1105 | `		/* Already numeric */` |
|    896126 | 1106 | `		return  SXRET_OK;` |
|         - | 1107 | `	}` |
|       773 | 1108 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       773 | 1109 | `		const char *zTail = 0;` |
|       773 | 1110 | `		int bNum, bReal = 0;` |
|         - | 1111 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1112 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1113 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1114 | `		 * php sees the prefix "1" there and yields int(1). */` |
|       773 | 1115 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|       773 | 1116 | `		if( bNum ){` |
|       773 | 1117 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      6357 | 1118 | `			while( z < zTail ){` |
|      5737 | 1119 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       152 | 1120 | `					bReal = 1;` |
|       152 | 1121 | `					break;` |
|         - | 1122 | `				}` |
|      5589 | 1123 | `				z++;` |
|         5 | 1124 | `			}` |
|       384 | 1125 | `		}` |
|       773 | 1126 | `		if( bReal ){` |
|       152 | 1127 | `			PH7_MemObjToReal(&(*pObj));` |
|        78 | 1128 | `		}else{` |
|       625 | 1129 | `			if( !bNum ){` |
|         - | 1130 | `				/* The input does not look at all like a number,set the value to 0 */` |
|       ! 0 | 1131 | `				pObj->x.iVal = 0;` |
|       ! 0 | 1132 | `			}else{` |
|       625 | 1133 | `				int iOverflow = 0;` |
|         - | 1134 | `				/* Convert as much as we can */` |
|       625 | 1135 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1136 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       625 | 1137 | `				if( iOverflow ){` |
|         - | 1138 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1139 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1140 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1141 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1142 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1143 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1144 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1145 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1146 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1147 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1148 | `					 * OP_ADD's overflow arm makes there. */` |
|       221 | 1149 | `					PH7_MemObjToReal(&(*pObj));` |
|       221 | 1150 | `					return SXRET_OK;` |
|         - | 1151 | `				}` |
|         - | 1152 | `#endif` |
|         - | 1153 | `			}` |
|       405 | 1154 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       405 | 1155 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1156 | `		}` |
|       274 | 1157 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 | 1158 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1159 | `	}else{` |
|         - | 1160 | `		/* Perform a blind cast */` |
|       ! 0 | 1161 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1162 | `	}` |
|       553 | 1163 | `	return SXRET_OK;` |
|    449197 | 1164 | `}` |
|         - | 1165 | `/*` |
|         - | 1166 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1167 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1168 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1169 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1170 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1171 | ` * last carried character. Empty strings become "1".` |
|         - | 1172 | ` *` |
|         - | 1173 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1174 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1175 | ` * a string even though it looks numeric.` |
|         - | 1176 | ` */` |
|       ! 0 | 1177 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1178 | `{` |
|         - | 1179 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1180 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1181 | `	sxu32 nLen, pos;` |
|         - | 1182 | `	sxu8 *zStr;` |
|       ! 0 | 1183 | `	int carry = 1;` |
|         - | 1184 | `	int ch;` |
|         - | 1185 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1186 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1187 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1188 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1189 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1190 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1191 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1192 | `	}` |
|       ! 0 | 1193 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1194 | `	if( nLen == 0 ){` |
|       ! 0 | 1195 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1196 | `		return SXRET_OK;` |
|         - | 1197 | `	}` |
|       ! 0 | 1198 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1199 | `	pos = nLen;` |
|       ! 0 | 1200 | `	while( pos > 0 ){` |
|       ! 0 | 1201 | `		pos--;` |
|       ! 0 | 1202 | `		ch = zStr[pos];` |
|       ! 0 | 1203 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1204 | `			if( ch == 'z' ){` |
|       ! 0 | 1205 | `				zStr[pos] = 'a';` |
|       ! 0 | 1206 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1207 | `				continue;` |
|         - | 1208 | `			}` |
|       ! 0 | 1209 | `			zStr[pos]++;` |
|       ! 0 | 1210 | `			carry = 0;` |
|       ! 0 | 1211 | `			break;` |
|       ! 0 | 1212 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1213 | `			if( ch == 'Z' ){` |
|       ! 0 | 1214 | `				zStr[pos] = 'A';` |
|       ! 0 | 1215 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1216 | `				continue;` |
|         - | 1217 | `			}` |
|       ! 0 | 1218 | `			zStr[pos]++;` |
|       ! 0 | 1219 | `			carry = 0;` |
|       ! 0 | 1220 | `			break;` |
|       ! 0 | 1221 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1222 | `			if( ch == '9' ){` |
|       ! 0 | 1223 | `				zStr[pos] = '0';` |
|       ! 0 | 1224 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1225 | `				continue;` |
|         - | 1226 | `			}` |
|       ! 0 | 1227 | `			zStr[pos]++;` |
|       ! 0 | 1228 | `			carry = 0;` |
|       ! 0 | 1229 | `			break;` |
|       ! 0 | 1230 | `		}else{` |
|         - | 1231 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1232 | `			carry = 0;` |
|       ! 0 | 1233 | `			break;` |
|         - | 1234 | `		}` |
|       ! 0 | 1235 | `	}` |
|       ! 0 | 1236 | `	if( carry ){` |
|         - | 1237 | `		sxu8 prepend;` |
|         - | 1238 | `		sxu32 i;` |
|       ! 0 | 1239 | `		switch( last_class ){` |
|       ! 0 | 1240 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1241 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1242 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1243 | `		}` |
|         - | 1244 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1245 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1246 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1247 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1248 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1249 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1250 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1251 | `		}` |
|       ! 0 | 1252 | `		zStr[0] = prepend;` |
|       ! 0 | 1253 | `	}` |
|       ! 0 | 1254 | `	return SXRET_OK;` |
|       ! 0 | 1255 | `}` |
|         - | 1256 | `/*` |
|         - | 1257 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1258 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1259 | ` */` |
|      1274 | 1260 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         4 | 1261 | `{` |
|      1278 | 1262 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1263 | `		/* Work only with reals */` |
|      1278 | 1264 | `		MemObjTryIntger(&(*pObj));` |
|       637 | 1265 | `	}` |
|      1278 | 1266 | `	return SXRET_OK;` |
|         4 | 1267 | `}` |
|         - | 1268 | `/*` |
|         - | 1269 | ` * Initialize a ph7_value to the null type.` |
|         - | 1270 | ` */` |
|  98820871 | 1271 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1272 | `{` |
|         - | 1273 | `	/* Zero the structure */` |
|  98820876 | 1274 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1275 | `	/* Initialize fields */` |
|  98820876 | 1276 | `	pObj->pVm = pVm;` |
|  98820876 | 1277 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1278 | `	/* Set the NULL type */` |
|  98820876 | 1279 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  98820876 | 1280 | `	return SXRET_OK;` |
|         5 | 1281 | `}` |
|         - | 1282 | `/*` |
|         - | 1283 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1284 | ` */` |
|   7411239 | 1285 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1286 | `{` |
|         - | 1287 | `	/* Zero the structure */` |
|   7411244 | 1288 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1289 | `	/* Initialize fields */` |
|   7411244 | 1290 | `	pObj->pVm = pVm;` |
|   7411244 | 1291 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1292 | `	/* Set the desired type */` |
|   7411244 | 1293 | `	pObj->x.iVal = iVal;` |
|   7411244 | 1294 | `	pObj->iFlags = MEMOBJ_INT;` |
|   7411244 | 1295 | `	return SXRET_OK;` |
|         5 | 1296 | `}` |
|         - | 1297 | `/*` |
|         - | 1298 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1299 | ` */` |
|     21768 | 1300 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1301 | `{` |
|         - | 1302 | `	/* Zero the structure */` |
|     21773 | 1303 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1304 | `	/* Initialize fields */` |
|     21773 | 1305 | `	pObj->pVm = pVm;` |
|     21773 | 1306 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1307 | `	/* Set the desired type */` |
|     21773 | 1308 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     21773 | 1309 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     21773 | 1310 | `	return SXRET_OK;` |
|         5 | 1311 | `}` |
|         - | 1312 | `/*` |
|         - | 1313 | ` * Initialize a ph7_value to the real type.` |
|         - | 1314 | ` */` |
|       132 | 1315 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         2 | 1316 | `{` |
|         - | 1317 | `	/* Zero the structure */` |
|       134 | 1318 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1319 | `	/* Initialize fields */` |
|       134 | 1320 | `	pObj->pVm = pVm;` |
|       134 | 1321 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1322 | `	/* Set the desired type */` |
|       134 | 1323 | `	pObj->rVal = rVal;` |
|       134 | 1324 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       134 | 1325 | `	return SXRET_OK;` |
|         2 | 1326 | `}` |
|         - | 1327 | `/*` |
|         - | 1328 | ` * Initialize a ph7_value to the array type.` |
|         - | 1329 | ` */` |
|   3607134 | 1330 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1331 | `{` |
|         - | 1332 | `	/* Zero the structure */` |
|   3607139 | 1333 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1334 | `	/* Initialize fields */` |
|   3607139 | 1335 | `	pObj->pVm = pVm;` |
|   3607139 | 1336 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1337 | `	/* Set the desired type */` |
|   3607139 | 1338 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   3607139 | 1339 | `	pObj->x.pOther = pArray;` |
|   3607139 | 1340 | `	return SXRET_OK;` |
|         5 | 1341 | `}` |
|         - | 1342 | `/*` |
|         - | 1343 | ` * Initialize a ph7_value to the string type.` |
|         - | 1344 | ` */` |
|  11382934 | 1345 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1346 | `{` |
|         - | 1347 | `	/* Zero the structure */` |
|  11382939 | 1348 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1349 | `	/* Initialize fields */` |
|  11382939 | 1350 | `	pObj->pVm = pVm;` |
|  11382939 | 1351 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  11382939 | 1352 | `	if( pVal ){` |
|         - | 1353 | `		/* Append contents */` |
|   8125873 | 1354 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   4062936 | 1355 | `	}` |
|         - | 1356 | `	/* Set the desired type */` |
|  11382939 | 1357 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  11382939 | 1358 | `	return SXRET_OK;` |
|         5 | 1359 | `}` |
|         - | 1360 | `/*` |
|         - | 1361 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1362 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1363 | ` * invalidate any prior representation and set the string type.` |
|         - | 1364 | ` * Then a simple append operation is performed.` |
|         - | 1365 | ` */` |
|   3518282 | 1366 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1367 | `{` |
|         - | 1368 | `	sxi32 rc;` |
|   3518287 | 1369 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1370 | `		/* Invalidate any prior representation */` |
|     18157 | 1371 | `		PH7_MemObjRelease(pObj);` |
|     18157 | 1372 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|      9076 | 1373 | `	}` |
|         - | 1374 | `	/* Append contents */` |
|   3518287 | 1375 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   3518287 | 1376 | `	return rc;` |
|         5 | 1377 | `}` |
|         - | 1378 | `#if 0` |
|         - | 1379 | `/*` |
|         - | 1380 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1381 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1382 | ` * any prior representation and set the string type.` |
|         - | 1383 | ` * Then a simple format and append operation is performed.` |
|         - | 1384 | ` */` |
|         - | 1385 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1386 | `{` |
|         - | 1387 | `	sxi32 rc;` |
|         - | 1388 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1389 | `		/* Invalidate any prior representation */` |
|         - | 1390 | `		PH7_MemObjRelease(pObj);` |
|         - | 1391 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1392 | `	}` |
|         - | 1393 | `	/* Format and append contents */` |
|         - | 1394 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1395 | `	return rc;` |
|         - | 1396 | `}` |
|         - | 1397 | `#endif` |
|         - | 1398 | `/*` |
|         - | 1399 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1400 | ` */` |
|  15263208 | 1401 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1402 | `{` |
|  15263213 | 1403 | `	ph7_class_instance *pObj = 0;` |
|  15263213 | 1404 | `	ph7_hashmap *pMap = 0;` |
|         - | 1405 | `	sxi32 rc;` |
|  15263213 | 1406 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1407 | `		/* Increment reference count */` |
|   2325661 | 1408 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  14100385 | 1409 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1410 | `		/* Increment reference count */` |
|     27477 | 1411 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     13736 | 1412 | `	}` |
|  15263213 | 1413 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|     91567 | 1414 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  15217432 | 1415 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|      9809 | 1416 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      4902 | 1417 | `	}` |
|  15263213 | 1418 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  15263213 | 1419 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  15263213 | 1420 | `	rc = SXRET_OK;` |
|  15263213 | 1421 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|   8730096 | 1422 | `		SyBlobReset(&pDest->sBlob);` |
|   8730096 | 1423 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   4366216 | 1424 | `	}else{` |
|   6533122 | 1425 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   1905251 | 1426 | `			SyBlobRelease(&pDest->sBlob);` |
|    953212 | 1427 | `		}` |
|         - | 1428 | `	}` |
|  15263213 | 1429 | `	if( pMap ){` |
|     91567 | 1430 | `		PH7_HashmapUnref(pMap);` |
|  15217432 | 1431 | `	}else if( pObj ){` |
|      9809 | 1432 | `		PH7_ClassInstanceUnref(pObj);` |
|      4902 | 1433 | `	}` |
|  15263208 | 1434 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|   8797579 | 1435 | `	 && pDest->pVm` |
|   2325656 | 1436 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1437 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1438 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1439 | `	  * for closure envs and other non-slot destinations. */` |
|   1162837 | 1440 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1441 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1442 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1443 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1444 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1445 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1446 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1447 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1448 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1449 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1450 | `			pDest->x.pOther = pSnap;` |
|         4 | 1451 | `		}else if( pSnap ){` |
|       ! 0 | 1452 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1453 | `		}` |
|         4 | 1454 | `	}` |
|  15263213 | 1455 | `	return rc;` |
|         5 | 1456 | `}` |
|         - | 1457 | `/*` |
|         - | 1458 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1459 | ` * buffer contents,simply point to it.` |
|         - | 1460 | ` */` |
|  17767631 | 1461 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1462 | `{` |
|  17767636 | 1463 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1464 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1465 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1466 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1467 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1468 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1469 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  17767636 | 1470 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  17767636 | 1471 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1472 | `		/* Increment reference count */` |
|    763333 | 1473 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  17385972 | 1474 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1475 | `		/* Increment reference count */` |
|    437186 | 1476 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    218592 | 1477 | `	}` |
|  17767636 | 1478 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|        83 | 1479 | `		SyBlobRelease(&pDest->sBlob);` |
|        39 | 1480 | `	}` |
|  17767636 | 1481 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  10268935 | 1482 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   5138376 | 1483 | `	}` |
|  17767636 | 1484 | `	return SXRET_OK;` |
|         5 | 1485 | `}` |
|         - | 1486 | `/*` |
|         - | 1487 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1488 | ` */` |
| 121612679 | 1489 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1490 | `{` |
| 121612684 | 1491 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1492 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1493 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1494 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1495 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1496 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1497 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1498 | `		pObj->x.pOther = 0;` |
|         7 | 1499 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1500 | `	}` |
| 121612684 | 1501 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - | 1502 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|         - | 1503 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|         - | 1504 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|         - | 1505 | `		 * argument list threw never leaks the receiver it was holding. */` |
|       ! 0 | 1506 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|       ! 0 | 1507 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1508 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       ! 0 | 1509 | `	}` |
| 121612684 | 1510 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1511 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1512 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1513 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1514 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|         3 | 1515 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|         3 | 1516 | `		pObj->x.pOther = 0;` |
|         3 | 1517 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|         1 | 1518 | `	}` |
| 121612684 | 1519 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  59258464 | 1520 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   6486503 | 1521 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  56015215 | 1522 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   5085106 | 1523 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   2542552 | 1524 | `		}` |
|         - | 1525 | `		/* Release the internal buffer */` |
|  59258464 | 1526 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1527 | `		/* Invalidate any prior representation */` |
|  59258464 | 1528 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  29642050 | 1529 | `	}` |
| 121612684 | 1530 | `	return SXRET_OK;` |
|         5 | 1531 | `}` |
|         - | 1532 | `/*` |
|         - | 1533 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1534 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1535 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1536 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1537 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1538 | ` *` |
|         - | 1539 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1540 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1541 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1542 | ` *` |
|         - | 1543 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1544 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1545 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1546 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1547 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1548 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1549 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1550 | ` */` |
|       106 | 1551 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         1 | 1552 | `{` |
|       107 | 1553 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       107 | 1554 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       107 | 1555 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|        65 | 1556 | `		if( PH7_MemObjIsNotStringable(pSelf) ){` |
|        13 | 1557 | `			return FALSE;` |
|         - | 1558 | `		}` |
|        53 | 1559 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        53 | 1560 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1561 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1562 | `			 * point; until then order the operands the way a refused cast does. */` |
|       ! 0 | 1563 | `			return FALSE;` |
|         - | 1564 | `		}` |
|        53 | 1565 | `		return TRUE;` |
|         - | 1566 | `	}` |
|        43 | 1567 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1568 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|         7 | 1569 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|         7 | 1570 | `		return TRUE;` |
|         - | 1571 | `	}` |
|        37 | 1572 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1573 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1574 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1575 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1576 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1577 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|        10 | 1578 | `		}` |
|        21 | 1579 | `		if( bReal ){` |
|         7 | 1580 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 1581 | `		}else{` |
|        15 | 1582 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 1583 | `		}` |
|        21 | 1584 | `		return TRUE;` |
|         - | 1585 | `	}` |
|        17 | 1586 | `	return FALSE;` |
|        54 | 1587 | `}` |
|         - | 1588 | `/*` |
|         - | 1589 | ` * Compare two ph7_values.` |
|         - | 1590 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1591 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1592 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1593 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1594 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1595 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1596 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1597 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1598 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1599 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1600 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1601 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1602 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1603 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1604 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1605 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1606 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1607 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1608 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1609 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1610 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1611 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1612 | ` *      Loose comparisons with ==` |
|         - | 1613 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1614 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1615 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1616 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1617 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1618 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1619 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1620 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1621 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1622 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1623 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1624 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1625 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1626 | ` *    Strict comparisons with ===` |
|         - | 1627 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1628 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1629 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1630 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1631 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1632 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1633 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1634 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1635 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1636 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1637 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1638 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1639 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1640 | ` */` |
|   2294103 | 1641 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1642 | `{` |
|         - | 1643 | `	sxi32 iComb;` |
|         - | 1644 | `	sxi32 rc;` |
|   2294108 | 1645 | `	if( bStrict ){` |
|         - | 1646 | `		sxi32 iF1,iF2;` |
|         - | 1647 | `		/* Strict comparisons with === */` |
|   1193716 | 1648 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   1193716 | 1649 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   1193716 | 1650 | `		if( iF1 != iF2 ){` |
|         - | 1651 | `			/* Not of the same type */` |
|    278097 | 1652 | `			return 1;` |
|         - | 1653 | `		}` |
|    458639 | 1654 | `	}` |
|         - | 1655 | `	/* Combine flag together */` |
|   2016016 | 1656 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   2016011 | 1657 | `	if( !bStrict` |
|   1559031 | 1658 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    550930 | 1659 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        69 | 1660 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1661 | `		/*` |
|         - | 1662 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1663 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1664 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1665 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1666 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1667 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1668 | `		 */` |
|        45 | 1669 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        35 | 1670 | `			PH7_MemObjToString(pObj1);` |
|        18 | 1671 | `		}else{` |
|        11 | 1672 | `			PH7_MemObjToString(pObj2);` |
|         - | 1673 | `		}` |
|        45 | 1674 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1675 | `	}` |
|   2016016 | 1676 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1677 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1678 | `		 * call every live resource equal to every other, since all are truthy. */` |
|         5 | 1679 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|         5 | 1680 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|         5 | 1681 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1682 | `	}` |
|   2016012 | 1683 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 1684 | `		/*` |
|         - | 1685 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 1686 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 1687 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 1688 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 1689 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 1690 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 1691 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 1692 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 1693 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 1694 | `		 * equalled every open resource.` |
|         - | 1695 | `		 *` |
|         - | 1696 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 1697 | `		 * already answered 1.` |
|         - | 1698 | `		 */` |
|       107 | 1699 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       107 | 1700 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       107 | 1701 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 1702 | `		ph7_value sCast;` |
|       107 | 1703 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 1704 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        73 | 1705 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        45 | 1706 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|        79 | 1707 | `			PH7_MemObjRelease(&sCast);` |
|        79 | 1708 | `			return rc;` |
|         - | 1709 | `		}` |
|        29 | 1710 | `		PH7_MemObjRelease(&sCast);` |
|         - | 1711 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|        29 | 1712 | `		return bObj1 ? 1 : -1;` |
|         - | 1713 | `	}` |
|   2015906 | 1714 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|         - | 1715 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|     48207 | 1716 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     30735 | 1717 | `			PH7_MemObjToBool(pObj1);` |
|     15365 | 1718 | `		}` |
|     48207 | 1719 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     29531 | 1720 | `			PH7_MemObjToBool(pObj2);` |
|     14763 | 1721 | `		}` |
|     48207 | 1722 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   1967704 | 1723 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1724 | `		/* Hashmap aka 'array' comparison */` |
|        89 | 1725 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1726 | `			/* Array is always greater */` |
|       ! 0 | 1727 | `			return -1;` |
|         - | 1728 | `		}` |
|        89 | 1729 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1730 | `			/* Array is always greater */` |
|       ! 0 | 1731 | `			return 1;` |
|         - | 1732 | `		}` |
|         - | 1733 | `		/* Perform the comparison */` |
|        89 | 1734 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|        89 | 1735 | `		return rc;` |
|   1967618 | 1736 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1737 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 1738 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 1739 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 1740 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|       373 | 1741 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1742 | `			/* Object is always greater */` |
|       ! 0 | 1743 | `			return -1;` |
|         - | 1744 | `		}` |
|       373 | 1745 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1746 | `			/* Object is always greater */` |
|       ! 0 | 1747 | `			return 1;` |
|         - | 1748 | `		}` |
|         - | 1749 | `		/* Perform the comparison */` |
|       373 | 1750 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       373 | 1751 | `		return rc;` |
|   1967250 | 1752 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 1753 | `		SyString s1,s2;` |
|   1152815 | 1754 | `		if( !bStrict ){` |
|         - | 1755 | `			/*` |
|         - | 1756 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 1757 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 1758 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 1759 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 1760 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 1761 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 1762 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 1763 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 1764 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 1765 | `			 * below, unchanged.` |
|         - | 1766 | `			 */` |
|    288146 | 1767 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 1768 | `				/*` |
|         - | 1769 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 1770 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 1771 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 1772 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 1773 | `				 * really does compare as doubles, so` |
|         - | 1774 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 1775 | `				 *` |
|         - | 1776 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 1777 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 1778 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 1779 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 1780 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 1781 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 1782 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 1783 | `				 *    one, no conversion involved -- which is why` |
|         - | 1784 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 1785 | `				 *    reach the same double.` |
|         - | 1786 | `				 *` |
|         - | 1787 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 1788 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 1789 | `				 */` |
|       336 | 1790 | `				int bBytes = 0;` |
|         - | 1791 | `				{` |
|       336 | 1792 | `					ph7_real r1 = 0, r2 = 0;` |
|       336 | 1793 | `					int iOf1 = 0, iOf2 = 0;` |
|       336 | 1794 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|       336 | 1795 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|       336 | 1796 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 1797 | `						bBytes = 1;` |
|       286 | 1798 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        37 | 1799 | `						return iOf1;` |
|       208 | 1800 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        17 | 1801 | `						return -iOf2;` |
|         - | 1802 | `					}` |
|         - | 1803 | `				}` |
|       292 | 1804 | `				if( !bBytes ){` |
|         - | 1805 | `					/* Perform a numeric comparison */` |
|       192 | 1806 | `					goto Numeric;` |
|         - | 1807 | `				}` |
|        50 | 1808 | `			}` |
|    143803 | 1809 | `		}` |
|         - | 1810 | `		/* Perform a strict string comparison.*/` |
|   1152581 | 1811 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        25 | 1812 | `			PH7_MemObjToString(pObj1);` |
|        12 | 1813 | `		}` |
|   1152581 | 1814 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        35 | 1815 | `			PH7_MemObjToString(pObj2);` |
|        17 | 1816 | `		}` |
|   1152581 | 1817 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   1152581 | 1818 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 1819 | `		/*` |
|         - | 1820 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 1821 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 1822 | `		 */` |
|   1152581 | 1823 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   1152581 | 1824 | `		if( rc == 0 ){` |
|    364959 | 1825 | `			if( s1.nByte != s2.nByte ){` |
|     21365 | 1826 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     10686 | 1827 | `			}` |
|    182487 | 1828 | `		}` |
|   1152581 | 1829 | `		return rc;` |
|    814440 | 1830 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    406374 | 1831 | `Numeric:` |
|         - | 1832 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|    814630 | 1833 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       134 | 1834 | `			PH7_MemObjToNumeric(pObj1);` |
|        66 | 1835 | `		}` |
|    814630 | 1836 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       148 | 1837 | `			PH7_MemObjToNumeric(pObj2);` |
|        73 | 1838 | `		}` |
|    814630 | 1839 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 1840 | `			/*` |
|         - | 1841 | `			 * Symisc eXtension to the PHP language:` |
|         - | 1842 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 1843 | `			 */` |
|         - | 1844 | `			ph7_real r1,r2;` |
|         - | 1845 | `			/* Compare as reals */` |
|       495 | 1846 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        27 | 1847 | `				PH7_MemObjToReal(pObj1);` |
|        13 | 1848 | `			}` |
|       495 | 1849 | `			r1 = pObj1->rVal;` |
|       495 | 1850 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        59 | 1851 | `				PH7_MemObjToReal(pObj2);` |
|        29 | 1852 | `			}` |
|       495 | 1853 | `			r2 = pObj2->rVal;` |
|       495 | 1854 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 1855 | `				/*` |
|         - | 1856 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|         - | 1857 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|         - | 1858 | `				 * any non-NaN numeric value.` |
|         - | 1859 | `				 */` |
|        52 | 1860 | `				if( PH7_IS_NAN(r1) ){` |
|        42 | 1861 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|         - | 1862 | `				}` |
|        11 | 1863 | `				return -1;` |
|         - | 1864 | `			}` |
|       445 | 1865 | `			if( r1 > r2 ){` |
|        67 | 1866 | `				return 1;` |
|       381 | 1867 | `			}else if( r1 < r2 ){` |
|       161 | 1868 | `				return -1;` |
|         - | 1869 | `			}` |
|       222 | 1870 | `			return 0;` |
|       ! 0 | 1871 | `		}else{` |
|         - | 1872 | `			/* Integer comparison */` |
|    814138 | 1873 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|      6944 | 1874 | `				return 1;` |
|    807199 | 1875 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|    799282 | 1876 | `				return -1;` |
|         - | 1877 | `			}` |
|      7922 | 1878 | `			return 0;` |
|         - | 1879 | `		}` |
|         - | 1880 | `	}` |
|         - | 1881 | `	/* NOT REACHED */` |
|       ! 0 | 1882 | `	return 0;` |
|   1148578 | 1883 | `}` |
|         - | 1884 | `/*` |
|         - | 1885 | ` * Perform an addition operation of two ph7_values.` |
|         - | 1886 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 1887 | ` * is that the '+' operator is overloaded.` |
|         - | 1888 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 1889 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 1890 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 1891 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 1892 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 1893 | ` * be ignored.` |
|         - | 1894 | ` * This function take care of handling all the scenarios.` |
|         - | 1895 | ` */` |
|     25752 | 1896 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 1897 | `{` |
|     25757 | 1898 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1899 | `			/* Arithemtic operation */` |
|     21147 | 1900 | `			PH7_MemObjToNumeric(pObj1);` |
|     21147 | 1901 | `			PH7_MemObjToNumeric(pObj2);` |
|     21147 | 1902 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 1903 | `				/* Floating point arithmetic */` |
|         - | 1904 | `				ph7_real a,b;` |
|       116 | 1905 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        31 | 1906 | `					PH7_MemObjToReal(pObj1);` |
|        15 | 1907 | `				}` |
|       116 | 1908 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        53 | 1909 | `					PH7_MemObjToReal(pObj2);` |
|        26 | 1910 | `				}` |
|       116 | 1911 | `				a = pObj1->rVal;` |
|       116 | 1912 | `				b = pObj2->rVal;` |
|       116 | 1913 | `				pObj1->rVal = a+b;` |
|       116 | 1914 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1915 | `				/* Try to get an integer representation also */` |
|       116 | 1916 | `				MemObjTryIntger(&(*pObj1));` |
|        59 | 1917 | `			}else{` |
|         - | 1918 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 1919 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 1920 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 1921 | `				sxi64 a,b,r;` |
|     21033 | 1922 | `				a = pObj1->x.iVal;` |
|     21033 | 1923 | `				b = pObj2->x.iVal;` |
|     21033 | 1924 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 1925 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         9 | 1926 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|         9 | 1927 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 1928 | `#else` |
|         - | 1929 | `					pObj1->x.iVal = r;` |
|         - | 1930 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1931 | `#endif` |
|         5 | 1932 | `				}else{` |
|     21025 | 1933 | `					pObj1->x.iVal = r;` |
|     21025 | 1934 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 1935 | `				}` |
|         - | 1936 | `			}` |
|     10576 | 1937 | `	}else{` |
|      4615 | 1938 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 1939 | `			ph7_hashmap *pMap;` |
|         - | 1940 | `			sxi32 rc;` |
|      4615 | 1941 | `			if( bAddStore ){` |
|         - | 1942 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 1943 | `				 */` |
|         3 | 1944 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1945 | `					/* Force a hashmap cast */` |
|       ! 0 | 1946 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 1947 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1948 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1949 | `						return rc;` |
|         - | 1950 | `					}` |
|       ! 0 | 1951 | `				}` |
|         - | 1952 | `				/* COW separate before in-place mutation */` |
|         3 | 1953 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         2 | 1954 | `			}else{` |
|         - | 1955 | `				/* Create a new hashmap */` |
|      4613 | 1956 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      4613 | 1957 | `				if( pMap == 0){` |
|       ! 0 | 1958 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 1959 | `					return SXERR_MEM;` |
|         - | 1960 | `				}` |
|         - | 1961 | `			}` |
|      4615 | 1962 | `			if( !bAddStore ){` |
|      4613 | 1963 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1964 | `					/* Perform a hashmap duplication */` |
|      4613 | 1965 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2309 | 1966 | `				}else{` |
|       ! 0 | 1967 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1968 | `						/* Simple insertion */` |
|       ! 0 | 1969 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 1970 | `					}` |
|         - | 1971 | `				}` |
|      2304 | 1972 | `			}` |
|         - | 1973 | `			/* Perform the union */` |
|      4615 | 1974 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      4615 | 1975 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2310 | 1976 | `			}else{` |
|       ! 0 | 1977 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1978 | `					/* Simple insertion */` |
|       ! 0 | 1979 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 1980 | `				}` |
|         - | 1981 | `			}` |
|         - | 1982 | `			/* Reflect the change */` |
|      4615 | 1983 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 1984 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 1985 | `			}` |
|      4615 | 1986 | `			pObj1->x.pOther = pMap;` |
|      4615 | 1987 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2305 | 1988 | `		}` |
|         - | 1989 | `	}` |
|     25757 | 1990 | `	return SXRET_OK;` |
|     12881 | 1991 | `}` |
|         - | 1992 | `/*` |
|         - | 1993 | ` * Return a printable representation of the type of a given` |
|         - | 1994 | ` * ph7_value.` |
|         - | 1995 | ` */` |
|       ! 0 | 1996 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|       ! 0 | 1997 | `{` |
|       ! 0 | 1998 | `	const char *zType = "";` |
|       ! 0 | 1999 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 2000 | `		zType = "null";` |
|       ! 0 | 2001 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 2002 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 2003 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 2004 | `		zType = "double";` |
|       ! 0 | 2005 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 2006 | `		zType = "int";` |
|       ! 0 | 2007 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 2008 | `		zType = "string";` |
|       ! 0 | 2009 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2010 | `		zType = "bool";` |
|       ! 0 | 2011 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|       ! 0 | 2012 | `		zType = "array";` |
|       ! 0 | 2013 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 2014 | `		zType = "object";` |
|       ! 0 | 2015 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 2016 | `		zType = "resource";` |
|       ! 0 | 2017 | `	}` |
|       ! 0 | 2018 | `	return zType;` |
|       ! 0 | 2019 | `}` |
|         - | 2020 | `/*` |
|         - | 2021 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 2022 | ` * Store the dump in the given blob.` |
|         - | 2023 | ` */` |
|         - | 2024 | `/*` |
|         - | 2025 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2026 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2027 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2028 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2029 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2030 | ` */` |
|       106 | 2031 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         5 | 2032 | `{` |
|         - | 2033 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2034 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2035 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2036 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2037 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2038 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2039 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2040 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2041 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       111 | 2042 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2043 | `#else` |
|         - | 2044 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2045 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2046 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2047 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2048 | `	}else{` |
|         - | 2049 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2050 | `	}` |
|         - | 2051 | `#endif` |
|       111 | 2052 | `}` |
|         - | 2053 | `/*` |
|         - | 2054 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2055 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2056 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2057 | ` */` |
|       376 | 2058 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         4 | 2059 | `{` |
|       380 | 2060 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|         7 | 2061 | `		return;` |
|         - | 2062 | `	}` |
|       374 | 2063 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2064 | `		if( pObj->x.iVal != 0 ){` |
|       ! 0 | 2065 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       ! 0 | 2066 | `		}` |
|       ! 0 | 2067 | `		return;` |
|         - | 2068 | `	}` |
|       374 | 2069 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2070 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2071 | `		 * non-strings into the output) */` |
|       228 | 2072 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       228 | 2073 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       112 | 2074 | `		}` |
|       228 | 2075 | `		return;` |
|         - | 2076 | `	}` |
|       150 | 2077 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       192 | 2078 | `}` |
|      6196 | 2079 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2080 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2081 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2082 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2083 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2084 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2085 | `	int nDepth,        /* Nesting level */` |
|         - | 2086 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2087 | `	)` |
|         5 | 2088 | `{` |
|      6201 | 2089 | `	sxi32 rc = SXRET_OK;` |
|         - | 2090 | `	int i;` |
|      6201 | 2091 | `	if( !ShowType ){` |
|         - | 2092 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2093 | `		 * containers render the Array/Object block (which the container` |
|         - | 2094 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       212 | 2095 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       156 | 2096 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2097 | `		}` |
|        60 | 2098 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        58 | 2099 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2100 | `		}` |
|         3 | 2101 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         3 | 2102 | `		return SXRET_OK;` |
|         - | 2103 | `	}` |
|         - | 2104 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2105 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2106 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     13001 | 2107 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      7013 | 2108 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      3509 | 2109 | `	}` |
|      5993 | 2110 | `	if( isRef ){` |
|        65 | 2111 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        31 | 2112 | `	}` |
|      5993 | 2113 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       194 | 2114 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       194 | 2115 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2116 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|         7 | 2117 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|         7 | 2118 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|         7 | 2119 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|         7 | 2120 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|         3 | 2121 | `			}` |
|         7 | 2122 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         7 | 2123 | `			return SXRET_OK;` |
|         - | 2124 | `		}` |
|       188 | 2125 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       188 | 2126 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       188 | 2127 | `		return rc;` |
|         - | 2128 | `	}` |
|      5803 | 2129 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       201 | 2130 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       201 | 2131 | `		return SXRET_OK;` |
|         - | 2132 | `	}` |
|      5607 | 2133 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       657 | 2134 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|       657 | 2135 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       657 | 2136 | `		return rc;` |
|         - | 2137 | `	}` |
|      4955 | 2138 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      1311 | 2139 | `		if( pObj->x.iVal != 0 ){` |
|       811 | 2140 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       408 | 2141 | `		}else{` |
|       505 | 2142 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2143 | `		}` |
|      1311 | 2144 | `		return SXRET_OK;` |
|         - | 2145 | `	}` |
|      3649 | 2146 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2147 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2148 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       111 | 2149 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       111 | 2150 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       111 | 2151 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       111 | 2152 | `		return SXRET_OK;` |
|         - | 2153 | `	}` |
|      3543 | 2154 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      1657 | 2155 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      1657 | 2156 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1657 | 2157 | `		return SXRET_OK;` |
|         - | 2158 | `	}` |
|      1891 | 2159 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      1891 | 2160 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      1891 | 2161 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      1727 | 2162 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       861 | 2163 | `		}` |
|      1891 | 2164 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      1891 | 2165 | `		return SXRET_OK;` |
|         - | 2166 | `	}` |
|       ! 0 | 2167 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2168 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2169 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2170 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2171 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2172 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2173 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2174 | `		return SXRET_OK;` |
|         - | 2175 | `	}` |
|         - | 2176 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2177 | `	{` |
|       ! 0 | 2178 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2179 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2180 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2181 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2182 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2183 | `	}` |
|       ! 0 | 2184 | `	return rc;` |
|      3103 | 2185 | `}` |
|         - | 2186 |  |
