# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1033/1189 lines (86.88%)

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
|      1334 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|      1339 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      1219 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|         - |   69 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|         - |   70 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|         - |   71 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|         - |   72 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|         - |   73 | `	 * A value that IS a float is a float whatever it has cached. */` |
|      1207 |   74 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      1133 |   75 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|       761 |   76 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       225 |   77 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        35 |   78 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        35 |   79 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   80 | `	return "unknown";` |
|       672 |   81 | `}` |
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
|     17044 |   99 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
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
|     17049 |  115 | `  ph7_real r = pObj->rVal;` |
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
|     17049 |  133 | `  if( PH7_IS_NAN(r) \|\| r < (ph7_real)minInt \|\| r >= -(ph7_real)minInt ){` |
|      1087 |  134 | `    return minInt;` |
|       ! 0 |  135 | `  }else{` |
|     15967 |  136 | `    return (sxi64)r;` |
|         - |  137 | `  }` |
|         - |  138 | `#endif` |
|      8527 |  139 | `}` |
|         - |  140 | `/*` |
|         - |  141 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  142 | ` * to a 64-bit integer.` |
|         - |  143 | ` */` |
|    681374 |  144 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  145 | `{` |
|    681379 |  146 | `	sxi64 iVal = 0;` |
|    681379 |  147 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  148 | `		return 0;` |
|         - |  149 | `	}` |
|    681379 |  150 | `	if( pVal->zString[0] == '0' ){` |
|         - |  151 | `		sxi32 c;` |
|    241185 |  152 | `		if( pVal->nByte == sizeof(char) ){` |
|    235463 |  153 | `			return 0;` |
|         - |  154 | `		}` |
|      5727 |  155 | `		c = pVal->zString[1];` |
|      5727 |  156 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  157 | `			/* Hex digit stream */` |
|       167 |  158 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      5645 |  159 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  160 | `			/* Binary digit stream */` |
|       285 |  161 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      5421 |  162 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  163 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  164 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  165 | `			if( pVal->nByte > 2 ){` |
|        21 |  166 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  167 | `			}` |
|        11 |  168 | `		}else{` |
|         - |  169 | `			/* Legacy octal digit stream (leading 0) */` |
|      5259 |  170 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  171 | `		}` |
|      2866 |  172 | `	}else{` |
|         - |  173 | `		/* Decimal digit stream */` |
|    440199 |  174 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  175 | `	}` |
|    445921 |  176 | `	return iVal;` |
|    340692 |  177 | `}` |
|         - |  178 | `/*` |
|         - |  179 | ` * TRUE when the numeric PREFIX that ends at zTail is float-SHAPED -- it carries` |
|         - |  180 | ` * a '.' or a complete exponent. This is php's is_numeric_string answering` |
|         - |  181 | ` * IS_DOUBLE, and it decides which of two entirely different readings the bytes` |
|         - |  182 | ` * get: an integer-shaped run is read from its DIGITS, a float-shaped one from` |
|         - |  183 | ` * the double they spell.` |
|         - |  184 | ` */` |
|      3556 |  185 | `static int MemObjNumericPrefixIsFloat(ph7_value *pObj,const char *zTail)` |
|         5 |  186 | `{` |
|      3561 |  187 | `	const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     53223 |  188 | `	while( z < zTail ){` |
|     49935 |  189 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       271 |  190 | `			return TRUE;` |
|         - |  191 | `		}` |
|     49667 |  192 | `		z++;` |
|         5 |  193 | `	}` |
|      3293 |  194 | `	return FALSE;` |
|      1771 |  195 | `}` |
|         - |  196 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  197 | `/*` |
|         - |  198 | ` * php's zend_dval_to_lval_cap: the double->int conversion a NUMERIC STRING` |
|         - |  199 | ` * takes, which is not the one a real float takes. This one SATURATES at the` |
|         - |  200 | ` * int64 bounds and answers 0 for a value that is not finite, where the cast of` |
|         - |  201 | ` * an actual float answers PHP_INT_MIN for every out-of-range case` |
|         - |  202 | ` * (MemObjRealToInt -- a recorded divergence). PHL has always` |
|         - |  203 | `` * saturated the integer-shaped overflow, so `(int)"99999999999999999999"` is`` |
|         - |  204 | ` * PHP_INT_MAX in both engines; this is the same rule for the shapes that reach` |
|         - |  205 | ` * it through a double.` |
|         - |  206 | ` */` |
|       188 |  207 | `static sxi64 MemObjRealToIntCap(ph7_real r)` |
|         2 |  208 | `{` |
|         - |  209 | `	/* NaN fails both comparisons and either infinity fails one of them, so this` |
|         - |  210 | `	 * screens all three without a libm predicate. */` |
|       190 |  211 | `	if( !(r >= -1.7976931348623157e308 && r <= 1.7976931348623157e308) ){` |
|        13 |  212 | `		return 0;` |
|         - |  213 | `	}` |
|       178 |  214 | `	if( r >= 9223372036854775808.0 ){    /* +2^63, exact in double space */` |
|        21 |  215 | `		return LARGEST_INT64;` |
|         - |  216 | `	}` |
|       158 |  217 | `	if( r < -9223372036854775808.0 ){` |
|         5 |  218 | `		return SMALLEST_INT64;` |
|         - |  219 | `	}` |
|       154 |  220 | `	return (sxi64)r;` |
|        96 |  221 | `}` |
|         - |  222 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  223 | `/*` |
|         - |  224 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  225 | ` * do at representing the value that pObj describes as a string` |
|         - |  226 | ` * representation.` |
|         - |  227 | ` */` |
|      2816 |  228 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  229 | `{` |
|      2821 |  230 | `	sxi64 iVal = 0;` |
|         - |  231 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      2821 |  232 | `	const char *zTail = 0;` |
|      2816 |  233 | `	if( PH7_MemObjStringNumericPrefix(pObj,&zTail)` |
|      2816 |  234 | `	 && MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|         - |  235 | `		/* A float-shaped string is a DOUBLE first and an int second, which is the` |
|         - |  236 | ``		 * only reading that makes `(int)"1e3"` the 1000 it says: reading its`` |
|         - |  237 | `		 * digits stops at the 'e' and answers the mantissa's integer part, so` |
|         - |  238 | `		 * "1e3" was 1, "1.5e2" was 1 and "-2e2" was -2. The '.' forms were wrong` |
|         - |  239 | `		 * the same way wherever the double rounds away from the digits --` |
|         - |  240 | ``		 * `(int)"0.9999999999999999999"` is 1, not 0. */`` |
|       190 |  241 | `		ph7_real rVal = 0.0;` |
|       190 |  242 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),` |
|         - |  243 | `			(void *)&rVal,0);` |
|       190 |  244 | `		if( pOverflow ){` |
|         - |  245 | `			/* php reports no overflow for a float-shaped string however large it` |
|         - |  246 | `			 * is: it was always going to be a double, so no digits were lost. */` |
|       ! 0 |  247 | `			*pOverflow = 0;` |
|       ! 0 |  248 | `		}` |
|       190 |  249 | `		return MemObjRealToIntCap(rVal);` |
|         - |  250 | `	}` |
|         - |  251 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  252 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  253 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  254 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      2633 |  255 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|      2633 |  256 | `	return iVal;` |
|      1405 |  257 | `}` |
|         - |  258 | `/*` |
|         - |  259 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  260 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  261 | ` * successfully called. Any other return value indicates failure.` |
|         - |  262 | ` */` |
|       934 |  263 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  264 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  265 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  266 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  267 | `	sxu32 nLen,                /* Method name length */` |
|         - |  268 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  269 | `	)` |
|         5 |  270 | `{` |
|         - |  271 | `	ph7_class_method *pMethod;` |
|         - |  272 | `	/* Check if the method is available */` |
|       939 |  273 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|       939 |  274 | `	if( pMethod == 0 ){` |
|         - |  275 | `		/* No such method */` |
|         6 |  276 | `		return SXERR_NOTFOUND;` |
|         - |  277 | `	}` |
|         - |  278 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  279 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  280 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  281 | `	 * printed "Object" after a caught __toString() throw). */` |
|       935 |  282 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       472 |  283 | `}` |
|         - |  284 | `/*` |
|         - |  285 | ` * Return some kind of integer value which is the best we can` |
|         - |  286 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  287 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  288 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  289 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  290 | ` * a integer and return that.` |
|         - |  291 | ` * If pObj represents a NULL value, return 0.` |
|         - |  292 | ` */` |
|      2000 |  293 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  294 | `{` |
|         - |  295 | `	sxi32 iFlags;` |
|      2005 |  296 | `	iFlags = pObj->iFlags;` |
|      2005 |  297 | `	if (iFlags & MEMOBJ_REAL ){` |
|        99 |  298 | `		return MemObjRealToInt(&(*pObj));` |
|      1909 |  299 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       469 |  300 | `		return pObj->x.iVal;` |
|      1445 |  301 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  302 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  303 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  304 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|      1389 |  305 | `		return MemObjStringToInt(&(*pObj),0);` |
|        59 |  306 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        34 |  307 | `		return 0;` |
|        26 |  308 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  309 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  310 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  311 | `		 * followed php; int/float did not.) */` |
|        11 |  312 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        11 |  313 | `		sxu32 n = pMap->nEntry;` |
|        11 |  314 | `		PH7_HashmapUnref(pMap);` |
|        11 |  315 | `		return n > 0 ? 1 : 0;` |
|        16 |  316 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  317 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  318 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  319 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|         7 |  320 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         7 |  321 | `		if( pInst && pInst->pClass ){` |
|        10 |  322 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         6 |  323 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|         3 |  324 | `		}` |
|         7 |  325 | `		PH7_ClassInstanceUnref(pInst);` |
|         7 |  326 | `		return 1;` |
|        10 |  327 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  328 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  329 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        10 |  330 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  331 | `	}` |
|         - |  332 | `	/* CANT HAPPEN */` |
|       ! 0 |  333 | `	return 0;` |
|      1005 |  334 | `}` |
|         - |  335 | `/*` |
|         - |  336 | ` * Return some kind of real value which is the best we can` |
|         - |  337 | ` * do at representing the value that pObj describes as a real.` |
|         - |  338 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  339 | ` * integer then the integer  is promoted to real and that value` |
|         - |  340 | ` * is returned.` |
|         - |  341 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  342 | ` * into a real and return that.` |
|         - |  343 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  344 | ` */` |
|     14086 |  345 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  346 | `{` |
|         - |  347 | `	sxi32 iFlags;` |
|     14091 |  348 | `	iFlags = pObj->iFlags;` |
|     14091 |  349 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  350 | `		return pObj->rVal;` |
|     14091 |  351 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      1421 |  352 | `		return (ph7_real)pObj->x.iVal;` |
|     12675 |  353 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  354 | `		SyString sString;` |
|         - |  355 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  356 | `		ph7_real rVal = 0;` |
|         - |  357 | `#else` |
|     12655 |  358 | `		ph7_real rVal = 0.0;` |
|         - |  359 | `#endif` |
|     12655 |  360 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     12655 |  361 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  362 | `			/* Convert as much as we can */` |
|         - |  363 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  364 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  365 | `#else` |
|     12651 |  366 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  367 | `#endif` |
|      6323 |  368 | `		}` |
|     12655 |  369 | `		return rVal;` |
|        22 |  370 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  371 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  372 | `		return 0;` |
|         - |  373 | `#else` |
|         9 |  374 | `		return 0.0;` |
|         - |  375 | `#endif` |
|        14 |  376 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  377 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  378 | `		 * branch above. */` |
|       ! 0 |  379 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  380 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  381 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  382 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        14 |  383 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  384 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        12 |  385 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        12 |  386 | `		if( pInst && pInst->pClass ){` |
|        17 |  387 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        10 |  388 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|         5 |  389 | `		}` |
|        12 |  390 | `		PH7_ClassInstanceUnref(pInst);` |
|        12 |  391 | `		return (ph7_real)1.0;` |
|         3 |  392 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  393 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  394 | `	}` |
|         - |  395 | `	/* NOT REACHED  */` |
|       ! 0 |  396 | `	return 0;` |
|      7048 |  397 | `}` |
|         - |  398 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  399 | `/*` |
|         - |  400 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  401 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  402 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  403 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  404 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  405 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  406 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  407 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  408 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  409 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  410 | ` */` |
|       638 |  411 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  412 | `{` |
|         - |  413 | `	sxi32 iExp,i;` |
|       643 |  414 | `	iExp = nLen - 1;` |
|      4839 |  415 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      4201 |  416 | `		iExp--;` |
|         5 |  417 | `	}` |
|       643 |  418 | `	if( iExp <= 0 ){` |
|       587 |  419 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  420 | `	}` |
|         - |  421 | `	{` |
|        58 |  422 | `		sxi32 iDig = iExp + 1;` |
|         - |  423 | `		sxi32 iFirst;` |
|        58 |  424 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|        58 |  425 | `			iDig++;` |
|        28 |  426 | `		}` |
|        58 |  427 | `		iFirst = iDig;` |
|        96 |  428 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|        70 |  429 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        27 |  430 | `			iFirst++;` |
|         1 |  431 | `		}` |
|        58 |  432 | `		if( iFirst > iDig ){` |
|        27 |  433 | `			sxi32 nStrip = iFirst - iDig;` |
|        79 |  434 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        53 |  435 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        27 |  436 | `			}` |
|        27 |  437 | `			nLen -= nStrip;` |
|        13 |  438 | `		}` |
|         - |  439 | `	}` |
|        58 |  440 | `	if( bGeneric ){` |
|        42 |  441 | `		int bHasDot = 0;` |
|        84 |  442 | `		for( i = 0 ; i < iExp ; i++ ){` |
|        56 |  443 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        23 |  444 | `		}` |
|        42 |  445 | `		if( !bHasDot ){` |
|       168 |  446 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       140 |  447 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        71 |  448 | `			}` |
|        30 |  449 | `			zBuf[iExp] = '.';` |
|        30 |  450 | `			zBuf[iExp+1] = '0';` |
|        30 |  451 | `			nLen += 2;` |
|        14 |  452 | `		}` |
|        20 |  453 | `	}` |
|        58 |  454 | `	return nLen;` |
|       324 |  455 | `}` |
|         - |  456 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  457 | `/*` |
|         - |  458 | ` * Return the string representation of a given ph7_value.` |
|         - |  459 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  460 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  461 | ` * left without a rendering of pObj.` |
|         - |  462 | ` */` |
|     72932 |  463 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  464 | `{` |
|     72937 |  465 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  466 | `		/* Handle special floating-point values first */` |
|       497 |  467 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|         3 |  468 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       496 |  469 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|         7 |  470 | `			if( pObj->rVal < 0.0 ){` |
|       ! 0 |  471 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|       ! 0 |  472 | `			}else{` |
|         7 |  473 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  474 | `			}` |
|         4 |  475 | `		}else{` |
|         - |  476 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  477 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  478 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  479 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  480 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  481 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  482 | `			 * exponent/fraction quirks. */` |
|         - |  483 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       489 |  484 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       489 |  485 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  486 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  487 | `			}` |
|       489 |  488 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       489 |  489 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  490 | `#else` |
|         - |  491 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  492 | `#endif` |
|         5 |  493 | `		}` |
|     72691 |  494 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     71049 |  495 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  496 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     36923 |  497 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       183 |  498 | `		if( bStrictBool ){` |
|         - |  499 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       183 |  500 | `			if( pObj->x.iVal ){` |
|        83 |  501 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        39 |  502 | `			}` |
|         - |  503 | `			/* false produces empty string, nothing to append */` |
|        94 |  504 | `		}else{` |
|         - |  505 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  506 | `			if( pObj->x.iVal ){` |
|       ! 0 |  507 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  508 | `			}else{` |
|       ! 0 |  509 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  510 | `			}` |
|         5 |  511 | `		}` |
|      1312 |  512 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       119 |  513 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       119 |  514 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      1166 |  515 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  516 | `		ph7_value sResult;` |
|         - |  517 | `		sxi32 rc;` |
|         - |  518 | `		/* Invoke the __toString() method if available */` |
|       939 |  519 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|       939 |  520 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  521 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|       939 |  522 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  523 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  524 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  525 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  526 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  527 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  528 | `			 * instance reference. */` |
|       192 |  529 | `			PH7_MemObjRelease(&sResult);` |
|       192 |  530 | `			return rc;` |
|         - |  531 | `		}` |
|       751 |  532 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  533 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  534 | `			 * value, and requiring a non-empty one sent` |
|         - |  535 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  536 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  537 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  538 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  539 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  540 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  541 | `			 * result no return-type check governs. */` |
|       747 |  542 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       376 |  543 | `		}else{` |
|         - |  544 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  545 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         6 |  546 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  547 | `		}` |
|       751 |  548 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       751 |  549 | `		PH7_MemObjRelease(&sResult);` |
|       548 |  550 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  551 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  552 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  553 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  554 | `	}` |
|     72749 |  555 | `	return SXRET_OK;` |
|     36471 |  556 | `}` |
|         - |  557 | `/*` |
|         - |  558 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  559 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  560 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  561 | ` * (php's exact set):` |
|         - |  562 | ` * NULL` |
|         - |  563 | ` * the boolean FALSE itself.` |
|         - |  564 | ` * the integer 0 (zero).` |
|         - |  565 | ` * the real 0.0 (zero).` |
|         - |  566 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  567 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  568 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  569 | ` * and were removed under the §10 PH7-ism policy).` |
|         - |  570 | ` * an array with zero elements.` |
|         - |  571 | ` */` |
|     67971 |  572 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  573 | `{` |
|         - |  574 | `	sxi32 iFlags;` |
|     67976 |  575 | `	iFlags = pObj->iFlags;` |
|     67976 |  576 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  577 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  578 | `		return pObj->rVal ? 1 : 0;` |
|         - |  579 | `#else` |
|        25 |  580 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  581 | `#endif` |
|     67954 |  582 | `	}else if( iFlags & MEMOBJ_INT ){` |
|       895 |  583 | `		return pObj->x.iVal ? 1 : 0;` |
|     67064 |  584 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  585 | `		SyString sString;` |
|       206 |  586 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  587 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|       206 |  588 | `		if( sString.nByte == 0 ){` |
|        71 |  589 | `			return 0;` |
|         - |  590 | `		}` |
|       138 |  591 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        14 |  592 | `			return 0;` |
|         - |  593 | `		}` |
|       126 |  594 | `		return 1;` |
|     66862 |  595 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     65443 |  596 | `		return 0;` |
|      1424 |  597 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        42 |  598 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        42 |  599 | `		sxu32 n = pMap->nEntry;` |
|        42 |  600 | `		PH7_HashmapUnref(pMap);` |
|        42 |  601 | `		return n > 0 ? TRUE : FALSE;` |
|      1384 |  602 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  603 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  604 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  605 | `		 * extension changed control flow in valid php source. */` |
|        75 |  606 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        75 |  607 | `		return 1;` |
|      1312 |  608 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1312 |  609 | `		return pObj->x.pOther != 0;` |
|         - |  610 | `	}` |
|         - |  611 | `	/* NOT REACHED */` |
|       ! 0 |  612 | `	return 0;` |
|     33989 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  616 | ` */` |
|     16948 |  617 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  618 | `{` |
|     16953 |  619 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  620 | `  /* Only mark the value as an integer if` |
|         - |  621 | `  **` |
|         - |  622 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  623 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  624 | `  **        possible integer` |
|         - |  625 | `  **` |
|         - |  626 | `  ** The second and third terms in the following conditional enforces` |
|         - |  627 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  628 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  629 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  630 | `  ** architectures might behave differently.` |
|         - |  631 | `  */` |
|     16948 |  632 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     12986 |  633 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     12961 |  634 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      6480 |  635 | `	}` |
|     16953 |  636 | `	return SXRET_OK;` |
|         5 |  637 | `}` |
|         - |  638 | `/*` |
|         - |  639 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  640 | ` */` |
|    829203 |  641 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  642 | `{` |
|    829208 |  643 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  644 | `		/* Preform the conversion */` |
|      2005 |  645 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  646 | `		/* Invalidate any prior representations */` |
|      2005 |  647 | `		SyBlobRelease(&pObj->sBlob);` |
|      2005 |  648 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      1000 |  649 | `	}` |
|    829208 |  650 | `	return SXRET_OK;` |
|         5 |  651 | `}` |
|         - |  652 | `/*` |
|         - |  653 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  654 | ` * Invalidate any prior representations` |
|         - |  655 | ` */` |
|     15408 |  656 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  657 | `{` |
|     15413 |  658 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  659 | `		/* Preform the conversion */` |
|     14091 |  660 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  661 | `		/* Invalidate any prior representations */` |
|     14091 |  662 | `		SyBlobRelease(&pObj->sBlob);` |
|     14091 |  663 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  664 | `		/* Try to get an integer representation */` |
|     14091 |  665 | `		MemObjTryIntger(&(*pObj));` |
|      7043 |  666 | `	}` |
|     15413 |  667 | `	return SXRET_OK;` |
|         5 |  668 | `}` |
|         - |  669 | `/*` |
|         - |  670 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  671 | ` */` |
|     86273 |  672 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  673 | `{` |
|     86278 |  674 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  675 | `		/* Preform the conversion */` |
|     67976 |  676 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  677 | `		/* Invalidate any prior representations */` |
|     67976 |  678 | `		SyBlobRelease(&pObj->sBlob);` |
|     67976 |  679 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     33984 |  680 | `	}` |
|     86278 |  681 | `	return SXRET_OK;` |
|         5 |  682 | `}` |
|         - |  683 | `/*` |
|         - |  684 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  685 | ` */` |
|   4451977 |  686 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  687 | `{` |
|   4451982 |  688 | `	sxi32 rc = SXRET_OK;` |
|   4451982 |  689 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  690 | `		/* Perform the conversion */` |
|     72777 |  691 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     72777 |  692 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     72777 |  693 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  694 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  695 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  696 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  697 | `			 * leaves $o holding the object after the throw is caught. */` |
|       192 |  698 | `			return rc;` |
|         - |  699 | `		}` |
|     72589 |  700 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     36292 |  701 | `	}` |
|   4451794 |  702 | `	return rc;` |
|   2227873 |  703 | `}` |
|         - |  704 | `/*` |
|         - |  705 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  706 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  707 | ` *   Error: Object of class X could not be converted to string` |
|         - |  708 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  709 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  710 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  711 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  712 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  713 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  714 | ` * cast still carried the placeholder.` |
|         - |  715 | ` *` |
|         - |  716 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  717 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - |  718 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - |  719 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - |  720 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - |  721 | ` * settype() site then blanks its target itself, because php's` |
|         - |  722 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - |  723 | ` */` |
|       560 |  724 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         5 |  725 | `{` |
|       565 |  726 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - |  727 | `	SyBlob sMsg;` |
|       565 |  728 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       565 |  729 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       560 |  730 | `		&pInst->pClass->sName);` |
|         - |  731 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       565 |  732 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         5 |  733 | `}` |
|         - |  734 | `/*` |
|         - |  735 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - |  736 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - |  737 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - |  738 | ` */` |
|     76124 |  739 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 |  740 | `{` |
|         - |  741 | `	ph7_class_instance *pInst;` |
|     76129 |  742 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|     74539 |  743 | `		return FALSE;` |
|         - |  744 | `	}` |
|      1595 |  745 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      1595 |  746 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 |  747 | `		return FALSE;` |
|         - |  748 | `	}` |
|      1595 |  749 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     38067 |  750 | `}` |
|         - |  751 | `/*` |
|         - |  752 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  753 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  754 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  755 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  756 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  757 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  758 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  759 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  760 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  761 | ` *` |
|         - |  762 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  763 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - |  764 | ` * ph7_value carries.` |
|         - |  765 | ` *` |
|         - |  766 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - |  767 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - |  768 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - |  769 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - |  770 | ` * exactly as php never throws for them.` |
|         - |  771 | ` *` |
|         - |  772 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - |  773 | ` */` |
|    904968 |  774 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  775 | `{` |
|    904973 |  776 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|    833987 |  777 | `		return SXRET_OK;` |
|         - |  778 | `	}` |
|     70991 |  779 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|        88 |  780 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        42 |  781 | `	}` |
|     70991 |  782 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       565 |  783 | `		return MemObjThrowNotStringable(pObj);` |
|         - |  784 | `	}` |
|     70431 |  785 | `	return PH7_MemObjToString(pObj);` |
|    452488 |  786 | `}` |
|         - |  787 | `/*` |
|         - |  788 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  789 | ` * representation.` |
|         - |  790 | ` */` |
|         2 |  791 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  792 | `{` |
|         3 |  793 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  794 | `}` |
|         - |  795 | `/*` |
|         - |  796 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  797 | `  * According to the PHP language reference manual.` |
|         - |  798 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  799 | `  *   to an array results in an array with a single element with index zero` |
|         - |  800 | `  *   and the value of the scalar which was converted.` |
|         - |  801 | `  */` |
|      2798 |  802 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  803 | `{` |
|      2803 |  804 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  805 | `		ph7_hashmap *pMap;` |
|         - |  806 | `		/* Allocate a new hashmap instance */` |
|      2229 |  807 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      2229 |  808 | `		if( pMap == 0 ){` |
|       ! 0 |  809 | `			return SXERR_MEM;` |
|         - |  810 | `		}` |
|      2229 |  811 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  812 | `			/*` |
|         - |  813 | `			 * According to the PHP language reference manual.` |
|         - |  814 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  815 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  816 | `			 *   and the value of the scalar which was converted.` |
|         - |  817 | `			 */` |
|       151 |  818 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       125 |  819 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       120 |  820 | `				if( pInst && pObj->pVm->pClosureClass` |
|       125 |  821 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|         - |  822 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|         - |  823 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|         - |  824 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|         - |  825 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|         - |  826 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|         3 |  827 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         2 |  828 | `				}else{` |
|         - |  829 | `					/* Object cast */` |
|       122 |  830 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|         - |  831 | `				}` |
|        65 |  832 | `			}else{` |
|         - |  833 | `				/* Insert a single element */` |
|        28 |  834 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  835 | `			}` |
|       151 |  836 | `			SyBlobRelease(&pObj->sBlob);` |
|        73 |  837 | `		}` |
|         - |  838 | `		/* Invalidate any prior representation */` |
|      2229 |  839 | `		PH7_MemObjRelease(pObj);` |
|      2229 |  840 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      2229 |  841 | `		pObj->x.pOther = pMap;` |
|      1112 |  842 | `	}` |
|      2803 |  843 | `	return SXRET_OK;` |
|      1404 |  844 | `}` |
|         - |  845 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  846 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  847 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  848 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|        92 |  849 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         3 |  850 | `{` |
|        95 |  851 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  852 | `	ph7_value *pSlot;` |
|         - |  853 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  854 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  855 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  856 | `	 * safe to coerce in place. */` |
|        95 |  857 | `	PH7_MemObjToString(pKey);` |
|       141 |  858 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|        92 |  859 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|        95 |  860 | `	if( pSlot ){` |
|        95 |  861 | `		PH7_MemObjStore(pValue,pSlot);` |
|        46 |  862 | `	}` |
|        95 |  863 | `	return SXRET_OK;` |
|         3 |  864 | `}` |
|         - |  865 | `/*` |
|         - |  866 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  867 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  868 | ` * matching PHP's (object) cast:` |
|         - |  869 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  870 | ` *   - scalar -> a single property named "scalar".` |
|         - |  871 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  872 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  873 | ` */` |
|        74 |  874 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         3 |  875 | `{` |
|        77 |  876 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  877 | `		ph7_class_instance *pStd;` |
|         - |  878 | `		ph7_class *pClass;` |
|         - |  879 | `		ph7_vm *pVm;` |
|         - |  880 | `		/* Point to the underlying VM + the stdClass */` |
|        77 |  881 | `		pVm = pObj->pVm;` |
|       114 |  882 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        37 |  883 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        77 |  884 | `		if( pClass == 0 ){` |
|         - |  885 | `			/* Can't happen,load null instead */` |
|       ! 0 |  886 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  887 | `			return SXRET_OK;` |
|         - |  888 | `		}` |
|         - |  889 | `		/* Instanciate a new (empty) stdClass object */` |
|        77 |  890 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|        77 |  891 | `		if( pStd == 0 ){` |
|         - |  892 | `			/* Out of memory */` |
|       ! 0 |  893 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  894 | `			return SXRET_OK;` |
|         - |  895 | `		}` |
|        77 |  896 | `		pStd->iRef = 1;` |
|        77 |  897 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - |  898 | `			/* Array: one dynamic property per entry. */` |
|         - |  899 | `			struct VmObjCastData sData;` |
|        63 |  900 | `			sData.pVm = pVm;` |
|        63 |  901 | `			sData.pStd = pStd;` |
|        63 |  902 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        46 |  903 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - |  904 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 |  905 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 |  906 | `			if( pSlot ){` |
|        14 |  907 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 |  908 | `			}` |
|         6 |  909 | `		}` |
|         - |  910 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - |  911 | `		/* Invalidate any prior representation */` |
|        77 |  912 | `		PH7_MemObjRelease(pObj);` |
|         - |  913 | `		/* Save the new instance */` |
|        77 |  914 | `		pObj->x.pOther = pStd;` |
|        77 |  915 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        37 |  916 | `	}` |
|        77 |  917 | `	return SXRET_OK;` |
|        40 |  918 | `}` |
|         - |  919 | `/*` |
|         - |  920 | ` * Return a pointer to the appropriate convertion method associated` |
|         - |  921 | ` * with the given type.` |
|         - |  922 | ` * Note on type juggling.` |
|         - |  923 | ` * Accoding to the PHP language reference manual` |
|         - |  924 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - |  925 | ` *  declaration; a variable's type is determined by the context in which` |
|         - |  926 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - |  927 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - |  928 | ` *  assigned to $var, it becomes an integer.` |
|         - |  929 | ` */` |
|    100258 |  930 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 |  931 | `{` |
|    100263 |  932 | `	if( iFlags & MEMOBJ_STRING ){` |
|        99 |  933 | `		return PH7_MemObjToString;` |
|    100169 |  934 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100113 |  935 | `		return PH7_MemObjToInteger;` |
|        60 |  936 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        51 |  937 | `		return PH7_MemObjToReal;` |
|        11 |  938 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 |  939 | `		return PH7_MemObjToBool;` |
|         8 |  940 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 |  941 | `		return PH7_MemObjToHashmap;` |
|       ! 0 |  942 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  943 | `		return PH7_MemObjToObject;` |
|       ! 0 |  944 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  945 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - |  946 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - |  947 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - |  948 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - |  949 | `		 * default. */` |
|       ! 0 |  950 | `		return 0;` |
|         - |  951 | `	}` |
|         - |  952 | `	/* NULL cast */` |
|       ! 0 |  953 | `	return PH7_MemObjToNull;` |
|     50134 |  954 | `}` |
|         - |  955 | `/*` |
|         - |  956 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - |  957 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - |  958 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - |  959 | ` * loose-comparison numeric gate:` |
|         - |  960 | ` *` |
|         - |  961 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - |  962 | ` *` |
|         - |  963 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - |  964 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - |  965 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - |  966 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - |  967 | ` * a non-string value.` |
|         - |  968 | ` */` |
|         - |  969 | `/*` |
|         - |  970 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - |  971 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - |  972 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - |  973 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - |  974 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - |  975 | ` * and rejects a string with no prefix outright.` |
|         - |  976 | ` */` |
|    413641 |  977 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 |  978 | `{` |
|         - |  979 | `	const char *z, *zEnd;` |
|         - |  980 | `	sxu32 n;` |
|    413646 |  981 | `	int bDigit = 0;` |
|    413646 |  982 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  983 | `		return 0;` |
|         - |  984 | `	}` |
|    413646 |  985 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|    413646 |  986 | `	n = SyBlobLength(&pValue->sBlob);` |
|    413646 |  987 | `	if( n == 0 ){` |
|       178 |  988 | `		return 0;` |
|         - |  989 | `	}` |
|    413472 |  990 | `	zEnd = z + n;` |
|    413800 |  991 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       332 |  992 | `		z++;` |
|         4 |  993 | `	}` |
|    413472 |  994 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       759 |  995 | `		z++;` |
|       377 |  996 | `	}` |
|    513534 |  997 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|    100067 |  998 | `		z++; bDigit = 1;` |
|         5 |  999 | `	}` |
|    413472 | 1000 | `	if( z < zEnd && z[0] == '.' ){` |
|      5883 | 1001 | `		z++;` |
|      7371 | 1002 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1493 | 1003 | `			z++; bDigit = 1;` |
|         5 | 1004 | `		}` |
|      3102 | 1005 | `	}` |
|         - | 1006 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|    413472 | 1007 | `	if( !bDigit ){` |
|    406440 | 1008 | `		return 0;` |
|         - | 1009 | `	}` |
|         - | 1010 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - | 1011 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|      7037 | 1012 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       319 | 1013 | `		const char *zExp = z;` |
|       319 | 1014 | `		z++;` |
|       319 | 1015 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|        23 | 1016 | `			z++;` |
|        11 | 1017 | `		}` |
|       319 | 1018 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        20 | 1019 | `			z = zExp;` |
|        11 | 1020 | `		}else{` |
|       709 | 1021 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       413 | 1022 | `				z++;` |
|         5 | 1023 | `			}` |
|         - | 1024 | `		}` |
|       157 | 1025 | `	}` |
|      7037 | 1026 | `	if( pzTail ){` |
|      7037 | 1027 | `		*pzTail = z;` |
|      3491 | 1028 | `	}` |
|      7037 | 1029 | `	return 1;` |
|    206686 | 1030 | `}` |
|         - | 1031 | `/*` |
|         - | 1032 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - | 1033 | ` * (trailing whitespace allowed, nothing else).` |
|         - | 1034 | ` */` |
|    408733 | 1035 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 | 1036 | `{` |
|    408738 | 1037 | `	const char *zTail = 0, *zEnd;` |
|    408738 | 1038 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    406538 | 1039 | `		return 0;` |
|         - | 1040 | `	}` |
|      2205 | 1041 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      2249 | 1042 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        47 | 1043 | `		zTail++;` |
|         3 | 1044 | `	}` |
|      2205 | 1045 | `	return zTail == zEnd ? 1 : 0;` |
|    204248 | 1046 | `}` |
|         - | 1047 | `/*` |
|         - | 1048 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - | 1049 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - | 1050 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - | 1051 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - | 1052 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - | 1053 | ` * those bytes convert to when it did.` |
|         - | 1054 | ` *` |
|         - | 1055 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - | 1056 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - | 1057 | ` * however large it is, because it was always going to be a double, so making it` |
|         - | 1058 | ` * one lost no digits.` |
|         - | 1059 | ` *` |
|         - | 1060 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - | 1061 | ` * intact when this says no.` |
|         - | 1062 | ` */` |
|       852 | 1063 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         3 | 1064 | `{` |
|       855 | 1065 | `	const char *zTail = 0;` |
|       855 | 1066 | `	int iOverflow = 0;` |
|       855 | 1067 | `	*piOverflow = 0;` |
|       855 | 1068 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|       104 | 1069 | `		return FALSE;` |
|         - | 1070 | `	}` |
|       753 | 1071 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 | 1072 | `		return FALSE;` |
|         - | 1073 | `	}` |
|         - | 1074 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - | 1075 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|       753 | 1076 | `	if( MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|        82 | 1077 | `		return FALSE;` |
|         - | 1078 | `	}` |
|       673 | 1079 | `	MemObjStringToInt(pObj,&iOverflow);` |
|       673 | 1080 | `	*piOverflow = iOverflow;` |
|       673 | 1081 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1082 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1083 | `	}` |
|       673 | 1084 | `	return TRUE;` |
|       425 | 1085 | `}` |
|         - | 1086 | `/*` |
|         - | 1087 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1088 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1089 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1090 | ` */` |
|    313809 | 1091 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1092 | `{` |
|    313814 | 1093 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      1685 | 1094 | `		return TRUE;` |
|    312134 | 1095 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      4725 | 1096 | `		return FALSE;` |
|    307414 | 1097 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1098 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    307414 | 1099 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1100 | `	}` |
|         - | 1101 | `	/* NOT REACHED */` |
|       ! 0 | 1102 | `	return FALSE;` |
|    156790 | 1103 | `}` |
|         - | 1104 | `/*` |
|         - | 1105 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1106 | ` * FALSE otherwise.` |
|         - | 1107 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1108 | ` * NULL value.` |
|         - | 1109 | ` * Boolean FALSE.` |
|         - | 1110 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1111 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1112 | ` * An empty array.` |
|         - | 1113 | ` * NOTE` |
|         - | 1114 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1115 | ` */` |
|     47768 | 1116 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1117 | `{` |
|     47773 | 1118 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        45 | 1119 | `		return TRUE;` |
|     47733 | 1120 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|        34 | 1121 | `		return pObj->x.iVal == 0 ? TRUE : FALSE;` |
|     47701 | 1122 | `	}else if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 1123 | `		return pObj->rVal == (ph7_real)0 ? TRUE : FALSE;` |
|     47701 | 1124 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|         5 | 1125 | `		return !pObj->x.iVal;` |
|     47697 | 1126 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     31629 | 1127 | `		if( SyBlobLength(&pObj->sBlob) <= 0 ){` |
|     24837 | 1128 | `			return TRUE;` |
|       ! 0 | 1129 | `		}else{` |
|         - | 1130 | `			const char *zIn,*zEnd;` |
|      6797 | 1131 | `			zIn = (const char *)SyBlobData(&pObj->sBlob);` |
|      6797 | 1132 | `			zEnd = &zIn[SyBlobLength(&pObj->sBlob)];` |
|      6805 | 1133 | `			while( zIn < zEnd ){` |
|      6805 | 1134 | `				if( zIn[0] != '0' ){` |
|      6797 | 1135 | `					break;` |
|         - | 1136 | `				}` |
|        10 | 1137 | `				zIn++;` |
|         2 | 1138 | `			}` |
|      6797 | 1139 | `			return zIn >= zEnd ? TRUE : FALSE;` |
|       ! 0 | 1140 | `		}` |
|     16073 | 1141 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     16073 | 1142 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|     16073 | 1143 | `		return pMap->nEntry == 0 ? TRUE : FALSE;` |
|       ! 0 | 1144 | `	}else if ( pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       ! 0 | 1145 | `		return FALSE;` |
|         - | 1146 | `	}` |
|         - | 1147 | `	/* Assume empty by default */` |
|       ! 0 | 1148 | `	return TRUE;` |
|     23889 | 1149 | `}` |
|         - | 1150 | `/*` |
|         - | 1151 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1152 | ` * or both.` |
|         - | 1153 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1154 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1155 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1156 | ` * and ignore the rest.` |
|         - | 1157 | ` */` |
|    934050 | 1158 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1159 | `{` |
|    934055 | 1160 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|    933103 | 1161 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        78 | 1162 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        60 | 1163 | `				pObj->x.iVal = 0;` |
|        28 | 1164 | `			}` |
|        78 | 1165 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        37 | 1166 | `		}` |
|         - | 1167 | `		/* Already numeric */` |
|    933103 | 1168 | `		return  SXRET_OK;` |
|         - | 1169 | `	}` |
|       957 | 1170 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|       957 | 1171 | `		const char *zTail = 0;` |
|       957 | 1172 | `		int bNum, bReal = 0;` |
|         - | 1173 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1174 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1175 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1176 | `		 * php sees the prefix "1" there and yields int(1). */` |
|       957 | 1177 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|       957 | 1178 | `		if( bNum ){` |
|       953 | 1179 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      7125 | 1180 | `			while( z < zTail ){` |
|      6363 | 1181 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       190 | 1182 | `					bReal = 1;` |
|       190 | 1183 | `					break;` |
|         - | 1184 | `				}` |
|      6177 | 1185 | `				z++;` |
|         5 | 1186 | `			}` |
|       470 | 1187 | `		}` |
|       957 | 1188 | `		if( bReal ){` |
|       190 | 1189 | `			PH7_MemObjToReal(&(*pObj));` |
|        97 | 1190 | `		}else{` |
|       771 | 1191 | `			if( !bNum ){` |
|         - | 1192 | `				/* The input does not look at all like a number,set the value to 0 */` |
|         5 | 1193 | `				pObj->x.iVal = 0;` |
|         3 | 1194 | `			}else{` |
|       767 | 1195 | `				int iOverflow = 0;` |
|         - | 1196 | `				/* Convert as much as we can */` |
|       767 | 1197 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1198 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       767 | 1199 | `				if( iOverflow ){` |
|         - | 1200 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1201 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1202 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1203 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1204 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1205 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1206 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1207 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1208 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1209 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1210 | `					 * OP_ADD's overflow arm makes there. */` |
|       228 | 1211 | `					PH7_MemObjToReal(&(*pObj));` |
|       228 | 1212 | `					return SXRET_OK;` |
|         - | 1213 | `				}` |
|         - | 1214 | `#endif` |
|         - | 1215 | `			}` |
|       545 | 1216 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       545 | 1217 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1218 | `		}` |
|       359 | 1219 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 | 1220 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1221 | `	}else{` |
|         - | 1222 | `		/* Perform a blind cast */` |
|       ! 0 | 1223 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1224 | `	}` |
|       731 | 1225 | `	return SXRET_OK;` |
|    467794 | 1226 | `}` |
|         - | 1227 | `/*` |
|         - | 1228 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1229 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1230 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1231 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1232 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1233 | ` * last carried character. Empty strings become "1".` |
|         - | 1234 | ` *` |
|         - | 1235 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1236 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1237 | ` * a string even though it looks numeric.` |
|         - | 1238 | ` */` |
|       ! 0 | 1239 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1240 | `{` |
|         - | 1241 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1242 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1243 | `	sxu32 nLen, pos;` |
|         - | 1244 | `	sxu8 *zStr;` |
|       ! 0 | 1245 | `	int carry = 1;` |
|         - | 1246 | `	int ch;` |
|         - | 1247 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1248 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1249 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1250 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1251 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1252 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1253 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1254 | `	}` |
|       ! 0 | 1255 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1256 | `	if( nLen == 0 ){` |
|       ! 0 | 1257 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1258 | `		return SXRET_OK;` |
|         - | 1259 | `	}` |
|       ! 0 | 1260 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1261 | `	pos = nLen;` |
|       ! 0 | 1262 | `	while( pos > 0 ){` |
|       ! 0 | 1263 | `		pos--;` |
|       ! 0 | 1264 | `		ch = zStr[pos];` |
|       ! 0 | 1265 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1266 | `			if( ch == 'z' ){` |
|       ! 0 | 1267 | `				zStr[pos] = 'a';` |
|       ! 0 | 1268 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1269 | `				continue;` |
|         - | 1270 | `			}` |
|       ! 0 | 1271 | `			zStr[pos]++;` |
|       ! 0 | 1272 | `			carry = 0;` |
|       ! 0 | 1273 | `			break;` |
|       ! 0 | 1274 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1275 | `			if( ch == 'Z' ){` |
|       ! 0 | 1276 | `				zStr[pos] = 'A';` |
|       ! 0 | 1277 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1278 | `				continue;` |
|         - | 1279 | `			}` |
|       ! 0 | 1280 | `			zStr[pos]++;` |
|       ! 0 | 1281 | `			carry = 0;` |
|       ! 0 | 1282 | `			break;` |
|       ! 0 | 1283 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1284 | `			if( ch == '9' ){` |
|       ! 0 | 1285 | `				zStr[pos] = '0';` |
|       ! 0 | 1286 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1287 | `				continue;` |
|         - | 1288 | `			}` |
|       ! 0 | 1289 | `			zStr[pos]++;` |
|       ! 0 | 1290 | `			carry = 0;` |
|       ! 0 | 1291 | `			break;` |
|       ! 0 | 1292 | `		}else{` |
|         - | 1293 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1294 | `			carry = 0;` |
|       ! 0 | 1295 | `			break;` |
|         - | 1296 | `		}` |
|       ! 0 | 1297 | `	}` |
|       ! 0 | 1298 | `	if( carry ){` |
|         - | 1299 | `		sxu8 prepend;` |
|         - | 1300 | `		sxu32 i;` |
|       ! 0 | 1301 | `		switch( last_class ){` |
|       ! 0 | 1302 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1303 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1304 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1305 | `		}` |
|         - | 1306 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1307 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1308 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1309 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1310 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1311 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1312 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1313 | `		}` |
|       ! 0 | 1314 | `		zStr[0] = prepend;` |
|       ! 0 | 1315 | `	}` |
|       ! 0 | 1316 | `	return SXRET_OK;` |
|       ! 0 | 1317 | `}` |
|         - | 1318 | `/*` |
|         - | 1319 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1320 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1321 | ` */` |
|      2746 | 1322 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         5 | 1323 | `{` |
|      2751 | 1324 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1325 | `		/* Work only with reals */` |
|      2751 | 1326 | `		MemObjTryIntger(&(*pObj));` |
|      1373 | 1327 | `	}` |
|      2751 | 1328 | `	return SXRET_OK;` |
|         5 | 1329 | `}` |
|         - | 1330 | `/*` |
|         - | 1331 | ` * Initialize a ph7_value to the null type.` |
|         - | 1332 | ` */` |
| 100676453 | 1333 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1334 | `{` |
|         - | 1335 | `	/* Zero the structure */` |
| 100676458 | 1336 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1337 | `	/* Initialize fields */` |
| 100676458 | 1338 | `	pObj->pVm = pVm;` |
| 100676458 | 1339 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1340 | `	/* Set the NULL type */` |
| 100676458 | 1341 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 100676458 | 1342 | `	return SXRET_OK;` |
|         5 | 1343 | `}` |
|         - | 1344 | `/*` |
|         - | 1345 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1346 | ` */` |
|   7464435 | 1347 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1348 | `{` |
|         - | 1349 | `	/* Zero the structure */` |
|   7464440 | 1350 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1351 | `	/* Initialize fields */` |
|   7464440 | 1352 | `	pObj->pVm = pVm;` |
|   7464440 | 1353 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1354 | `	/* Set the desired type */` |
|   7464440 | 1355 | `	pObj->x.iVal = iVal;` |
|   7464440 | 1356 | `	pObj->iFlags = MEMOBJ_INT;` |
|   7464440 | 1357 | `	return SXRET_OK;` |
|         5 | 1358 | `}` |
|         - | 1359 | `/*` |
|         - | 1360 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1361 | ` */` |
|     12774 | 1362 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1363 | `{` |
|         - | 1364 | `	/* Zero the structure */` |
|     12779 | 1365 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1366 | `	/* Initialize fields */` |
|     12779 | 1367 | `	pObj->pVm = pVm;` |
|     12779 | 1368 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1369 | `	/* Set the desired type */` |
|     12779 | 1370 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     12779 | 1371 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     12779 | 1372 | `	return SXRET_OK;` |
|         5 | 1373 | `}` |
|         - | 1374 | `/*` |
|         - | 1375 | ` * Initialize a ph7_value to the real type.` |
|         - | 1376 | ` */` |
|       132 | 1377 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         2 | 1378 | `{` |
|         - | 1379 | `	/* Zero the structure */` |
|       134 | 1380 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1381 | `	/* Initialize fields */` |
|       134 | 1382 | `	pObj->pVm = pVm;` |
|       134 | 1383 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1384 | `	/* Set the desired type */` |
|       134 | 1385 | `	pObj->rVal = rVal;` |
|       134 | 1386 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       134 | 1387 | `	return SXRET_OK;` |
|         2 | 1388 | `}` |
|         - | 1389 | `/*` |
|         - | 1390 | ` * Initialize a ph7_value to the array type.` |
|         - | 1391 | ` */` |
|   3618130 | 1392 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1393 | `{` |
|         - | 1394 | `	/* Zero the structure */` |
|   3618135 | 1395 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1396 | `	/* Initialize fields */` |
|   3618135 | 1397 | `	pObj->pVm = pVm;` |
|   3618135 | 1398 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1399 | `	/* Set the desired type */` |
|   3618135 | 1400 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   3618135 | 1401 | `	pObj->x.pOther = pArray;` |
|   3618135 | 1402 | `	return SXRET_OK;` |
|         5 | 1403 | `}` |
|         - | 1404 | `/*` |
|         - | 1405 | ` * Initialize a ph7_value to the string type.` |
|         - | 1406 | ` */` |
|  11544166 | 1407 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1408 | `{` |
|         - | 1409 | `	/* Zero the structure */` |
|  11544171 | 1410 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1411 | `	/* Initialize fields */` |
|  11544171 | 1412 | `	pObj->pVm = pVm;` |
|  11544171 | 1413 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  11544171 | 1414 | `	if( pVal ){` |
|         - | 1415 | `		/* Append contents */` |
|   8189569 | 1416 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   4094781 | 1417 | `	}` |
|         - | 1418 | `	/* Set the desired type */` |
|  11544171 | 1419 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  11544171 | 1420 | `	return SXRET_OK;` |
|         5 | 1421 | `}` |
|         - | 1422 | `/*` |
|         - | 1423 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1424 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1425 | ` * invalidate any prior representation and set the string type.` |
|         - | 1426 | ` * Then a simple append operation is performed.` |
|         - | 1427 | ` */` |
|   3812505 | 1428 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1429 | `{` |
|         - | 1430 | `	sxi32 rc;` |
|   3812510 | 1431 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1432 | `		/* Invalidate any prior representation */` |
|     27279 | 1433 | `		PH7_MemObjRelease(pObj);` |
|     27279 | 1434 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     13637 | 1435 | `	}` |
|         - | 1436 | `	/* Append contents */` |
|   3812510 | 1437 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   3812510 | 1438 | `	return rc;` |
|         5 | 1439 | `}` |
|         - | 1440 | `#if 0` |
|         - | 1441 | `/*` |
|         - | 1442 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1443 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1444 | ` * any prior representation and set the string type.` |
|         - | 1445 | ` * Then a simple format and append operation is performed.` |
|         - | 1446 | ` */` |
|         - | 1447 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1448 | `{` |
|         - | 1449 | `	sxi32 rc;` |
|         - | 1450 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1451 | `		/* Invalidate any prior representation */` |
|         - | 1452 | `		PH7_MemObjRelease(pObj);` |
|         - | 1453 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1454 | `	}` |
|         - | 1455 | `	/* Format and append contents */` |
|         - | 1456 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1457 | `	return rc;` |
|         - | 1458 | `}` |
|         - | 1459 | `#endif` |
|         - | 1460 | `/*` |
|         - | 1461 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1462 | ` */` |
|  16770040 | 1463 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1464 | `{` |
|  16770045 | 1465 | `	ph7_class_instance *pObj = 0;` |
|  16770045 | 1466 | `	ph7_hashmap *pMap = 0;` |
|         - | 1467 | `	sxi32 rc;` |
|  16770045 | 1468 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1469 | `		/* Increment reference count */` |
|   2348278 | 1470 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  15595907 | 1471 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1472 | `		/* Increment reference count */` |
|     33537 | 1473 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     16766 | 1474 | `	}` |
|  16770045 | 1475 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|     98742 | 1476 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  16720676 | 1477 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     12041 | 1478 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      6018 | 1479 | `	}` |
|  16770045 | 1480 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  16770045 | 1481 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  16770045 | 1482 | `	rc = SXRET_OK;` |
|  16770045 | 1483 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  10049990 | 1484 | `		SyBlobReset(&pDest->sBlob);` |
|  10049990 | 1485 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   5026179 | 1486 | `	}else{` |
|   6720060 | 1487 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   1947538 | 1488 | `			SyBlobRelease(&pDest->sBlob);` |
|    974376 | 1489 | `		}` |
|         - | 1490 | `	}` |
|  16770045 | 1491 | `	if( pMap ){` |
|     98742 | 1492 | `		PH7_HashmapUnref(pMap);` |
|  16720676 | 1493 | `	}else if( pObj ){` |
|     12041 | 1494 | `		PH7_ClassInstanceUnref(pObj);` |
|      6018 | 1495 | `	}` |
|  16770040 | 1496 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|   9562386 | 1497 | `	 && pDest->pVm` |
|   2348273 | 1498 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1499 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1500 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1501 | `	  * for closure envs and other non-slot destinations. */` |
|   1174144 | 1502 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1503 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1504 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1505 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1506 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1507 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1508 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1509 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1510 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1511 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1512 | `			pDest->x.pOther = pSnap;` |
|         4 | 1513 | `		}else if( pSnap ){` |
|       ! 0 | 1514 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1515 | `		}` |
|         4 | 1516 | `	}` |
|  16770045 | 1517 | `	return rc;` |
|         5 | 1518 | `}` |
|         - | 1519 | `/*` |
|         - | 1520 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1521 | ` * buffer contents,simply point to it.` |
|         - | 1522 | ` */` |
|  18638296 | 1523 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1524 | `{` |
|  18638301 | 1525 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1526 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1527 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1528 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1529 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1530 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1531 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  18638301 | 1532 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  18638301 | 1533 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1534 | `		/* Increment reference count */` |
|    815799 | 1535 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  18230399 | 1536 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1537 | `		/* Increment reference count */` |
|    441440 | 1538 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    220719 | 1539 | `	}` |
|  18638301 | 1540 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       209 | 1541 | `		SyBlobRelease(&pDest->sBlob);` |
|       102 | 1542 | `	}` |
|  18638301 | 1543 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  10822756 | 1544 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   5415480 | 1545 | `	}` |
|  18638301 | 1546 | `	return SXRET_OK;` |
|         5 | 1547 | `}` |
|         - | 1548 | `/*` |
|         - | 1549 | ` * Read a value WITHOUT converting the caller's copy of it.` |
|         - | 1550 | ` *` |
|         - | 1551 | ` * Every ph7_value_to_xxx()/PH7_MemObjToXxx() is destructive: it rewrites the` |
|         - | 1552 | ` * object it is handed and throws the prior representation away. That is right` |
|         - | 1553 | ` * for a VM operand, and wrong for an entry a builtin FETCHED out of an array` |
|         - | 1554 | ` * the script still holds — an $options member, a stream-filter parameter, a` |
|         - | 1555 | ` * proc_open descriptor — where converting in place rewrites the script's own` |
|         - | 1556 | ` * array (php's zval_get_long()/zval_get_string() family never touch theirs).` |
|         - | 1557 | ` *` |
|         - | 1558 | ` * PH7_ValuePeek loads an aliasing copy into pScratch (which the caller must` |
|         - | 1559 | ` * have PH7_MemObjInit'd and must PH7_MemObjRelease afterwards) and answers it,` |
|         - | 1560 | ` * so the destructive conversion lands on the copy. A string read through it` |
|         - | 1561 | ` * stays valid until the scratch value is released. The three scalar wrappers` |
|         - | 1562 | ` * carry their own scratch for the common case.` |
|         - | 1563 | ` */` |
|       150 | 1564 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch)` |
|         3 | 1565 | `{` |
|       153 | 1566 | `	PH7_MemObjLoad(pVal,pScratch);` |
|       153 | 1567 | `	return pScratch;` |
|         3 | 1568 | `}` |
|      1432 | 1569 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal)` |
|         5 | 1570 | `{` |
|         - | 1571 | `	ph7_value sTmp;` |
|         - | 1572 | `	sxi64 iVal;` |
|      1437 | 1573 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|      1437 | 1574 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|      1437 | 1575 | `	PH7_MemObjToInteger(&sTmp);` |
|      1437 | 1576 | `	iVal = sTmp.x.iVal;` |
|      1437 | 1577 | `	PH7_MemObjRelease(&sTmp);` |
|      1437 | 1578 | `	return iVal;` |
|         5 | 1579 | `}` |
|         - | 1580 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        16 | 1581 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal)` |
|         1 | 1582 | `{` |
|         - | 1583 | `	ph7_value sTmp;` |
|         - | 1584 | `	ph7_real rVal;` |
|        17 | 1585 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|        17 | 1586 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|        17 | 1587 | `	PH7_MemObjToReal(&sTmp);` |
|        17 | 1588 | `	rVal = sTmp.rVal;` |
|        17 | 1589 | `	PH7_MemObjRelease(&sTmp);` |
|        17 | 1590 | `	return rVal;` |
|         1 | 1591 | `}` |
|         - | 1592 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         6 | 1593 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal)` |
|         2 | 1594 | `{` |
|         - | 1595 | `	ph7_value sTmp;` |
|         - | 1596 | `	int bVal;` |
|         8 | 1597 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|         8 | 1598 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|         8 | 1599 | `	PH7_MemObjToBool(&sTmp);` |
|         8 | 1600 | `	bVal = sTmp.x.iVal != 0;` |
|         8 | 1601 | `	PH7_MemObjRelease(&sTmp);` |
|         8 | 1602 | `	return bVal;` |
|         2 | 1603 | `}` |
|         - | 1604 | `/*` |
|         - | 1605 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1606 | ` */` |
| 125259756 | 1607 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1608 | `{` |
| 125259761 | 1609 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1610 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1611 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1612 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1613 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1614 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1615 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1616 | `		pObj->x.pOther = 0;` |
|         7 | 1617 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1618 | `	}` |
| 125259761 | 1619 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - | 1620 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|         - | 1621 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|         - | 1622 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|         - | 1623 | `		 * argument list threw never leaks the receiver it was holding. */` |
|       ! 0 | 1624 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|       ! 0 | 1625 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1626 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       ! 0 | 1627 | `	}` |
| 125259761 | 1628 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1629 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1630 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1631 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1632 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|         3 | 1633 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|         3 | 1634 | `		pObj->x.pOther = 0;` |
|         3 | 1635 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|         1 | 1636 | `	}` |
| 125259761 | 1637 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  61832719 | 1638 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   6562688 | 1639 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  58551371 | 1640 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   5097034 | 1641 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   2548516 | 1642 | `		}` |
|         - | 1643 | `		/* Release the internal buffer */` |
|  61832719 | 1644 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1645 | `		/* Invalidate any prior representation */` |
|  61832719 | 1646 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  30929642 | 1647 | `	}` |
| 125259761 | 1648 | `	return SXRET_OK;` |
|         5 | 1649 | `}` |
|         - | 1650 | `/*` |
|         - | 1651 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1652 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1653 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1654 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1655 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1656 | ` *` |
|         - | 1657 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1658 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1659 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1660 | ` *` |
|         - | 1661 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1662 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1663 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1664 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1665 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1666 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1667 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1668 | ` */` |
|       192 | 1669 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         4 | 1670 | `{` |
|       196 | 1671 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       196 | 1672 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       196 | 1673 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|       150 | 1674 | `		if( PH7_MemObjIsNotStringable(pSelf)` |
|       142 | 1675 | `		 \|\| (pSelf->pVm && PH7_CALLBACK_UNWOUND(pSelf->pVm->nBoundaryRc)) ){` |
|         - | 1676 | `			/* php enters no PHP function while an exception is pending` |
|         - | 1677 | `			 * (zend_call_function bails on EG(exception)), so a __toString()` |
|         - | 1678 | `			 * that already threw -- or exited -- is NOT run again: every later` |
|         - | 1679 | `			 * comparison orders this operand the way a refused cast does. A sort` |
|         - | 1680 | `			 * used to re-enter the body once per remaining pair, and where the` |
|         - | 1681 | `			 * enclosing catch had already run in place the second throw was` |
|         - | 1682 | `			 * UNCAUGHT and killed the script. */` |
|        64 | 1683 | `			return FALSE;` |
|         - | 1684 | `		}` |
|        92 | 1685 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        92 | 1686 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1687 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1688 | `			 * point; until then order the operands the way a refused cast does. */` |
|        39 | 1689 | `			return FALSE;` |
|         - | 1690 | `		}` |
|        53 | 1691 | `		return TRUE;` |
|         - | 1692 | `	}` |
|        43 | 1693 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1694 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|         7 | 1695 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|         7 | 1696 | `		return TRUE;` |
|         - | 1697 | `	}` |
|        37 | 1698 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1699 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1700 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1701 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1702 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1703 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|        10 | 1704 | `		}` |
|        21 | 1705 | `		if( bReal ){` |
|         7 | 1706 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 1707 | `		}else{` |
|        15 | 1708 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 1709 | `		}` |
|        21 | 1710 | `		return TRUE;` |
|         - | 1711 | `	}` |
|        17 | 1712 | `	return FALSE;` |
|       100 | 1713 | `}` |
|         - | 1714 | `/*` |
|         - | 1715 | ` * Compare two ph7_values.` |
|         - | 1716 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1717 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1718 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1719 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1720 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1721 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1722 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1723 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1724 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1725 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1726 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1727 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1728 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1729 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1730 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1731 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1732 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1733 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1734 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1735 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1736 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1737 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1738 | ` *      Loose comparisons with ==` |
|         - | 1739 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1740 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1741 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1742 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1743 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1744 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1745 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1746 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1747 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1748 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1749 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1750 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1751 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1752 | ` *    Strict comparisons with ===` |
|         - | 1753 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1754 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1755 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1756 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1757 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1758 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1759 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1760 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1761 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1762 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1763 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1764 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1765 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1766 | ` */` |
|   2471707 | 1767 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1768 | `{` |
|         - | 1769 | `	sxi32 iComb;` |
|         - | 1770 | `	sxi32 rc;` |
|   2471712 | 1771 | `	if( bStrict ){` |
|         - | 1772 | `		sxi32 iF1,iF2;` |
|         - | 1773 | `		/* Strict comparisons with === */` |
|   1313653 | 1774 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   1313653 | 1775 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   1313653 | 1776 | `		if( iF1 != iF2 ){` |
|         - | 1777 | `			/* Not of the same type */` |
|    313275 | 1778 | `			return 1;` |
|         - | 1779 | `		}` |
|    501039 | 1780 | `	}` |
|         - | 1781 | `	/* Combine flag together */` |
|   2158442 | 1782 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   2158437 | 1783 | `	if( !bStrict` |
|   1659098 | 1784 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    579824 | 1785 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        69 | 1786 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1787 | `		/*` |
|         - | 1788 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1789 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1790 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1791 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1792 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1793 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1794 | `		 */` |
|        45 | 1795 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        35 | 1796 | `			PH7_MemObjToString(pObj1);` |
|        18 | 1797 | `		}else{` |
|        11 | 1798 | `			PH7_MemObjToString(pObj2);` |
|         - | 1799 | `		}` |
|        45 | 1800 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1801 | `	}` |
|   2158442 | 1802 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1803 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1804 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        22 | 1805 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        22 | 1806 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        22 | 1807 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1808 | `	}` |
|   2158422 | 1809 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 1810 | `		/*` |
|         - | 1811 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 1812 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 1813 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 1814 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 1815 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 1816 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 1817 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 1818 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 1819 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 1820 | `		 * equalled every open resource.` |
|         - | 1821 | `		 *` |
|         - | 1822 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 1823 | `		 * already answered 1.` |
|         - | 1824 | `		 */` |
|       196 | 1825 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       196 | 1826 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       196 | 1827 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 1828 | `		ph7_value sCast;` |
|       196 | 1829 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 1830 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        73 | 1831 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        45 | 1832 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|        79 | 1833 | `			PH7_MemObjRelease(&sCast);` |
|        79 | 1834 | `			return rc;` |
|         - | 1835 | `		}` |
|       118 | 1836 | `		PH7_MemObjRelease(&sCast);` |
|         - | 1837 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|       118 | 1838 | `		return bObj1 ? 1 : -1;` |
|         - | 1839 | `	}` |
|   2158230 | 1840 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_RES\|MEMOBJ_BOOL) ){` |
|         - | 1841 | `		/* Convert to boolean: Keep in mind FALSE < TRUE */` |
|     57731 | 1842 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     33963 | 1843 | `			PH7_MemObjToBool(pObj1);` |
|     16978 | 1844 | `		}` |
|     57731 | 1845 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     32696 | 1846 | `			PH7_MemObjToBool(pObj2);` |
|     16345 | 1847 | `		}` |
|     57731 | 1848 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   2100504 | 1849 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1850 | `		/* Hashmap aka 'array' comparison */` |
|       208 | 1851 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1852 | `			/* Array is always greater */` |
|       ! 0 | 1853 | `			return -1;` |
|         - | 1854 | `		}` |
|       208 | 1855 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1856 | `			/* Array is always greater */` |
|       ! 0 | 1857 | `			return 1;` |
|         - | 1858 | `		}` |
|         - | 1859 | `		/* Perform the comparison */` |
|       208 | 1860 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       208 | 1861 | `		return rc;` |
|   2100300 | 1862 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1863 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 1864 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 1865 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 1866 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|       397 | 1867 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1868 | `			/* Object is always greater */` |
|       ! 0 | 1869 | `			return -1;` |
|         - | 1870 | `		}` |
|       397 | 1871 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1872 | `			/* Object is always greater */` |
|       ! 0 | 1873 | `			return 1;` |
|         - | 1874 | `		}` |
|         - | 1875 | `		/* Perform the comparison */` |
|       397 | 1876 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       397 | 1877 | `		return rc;` |
|   2099908 | 1878 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 1879 | `		SyString s1,s2;` |
|   1244559 | 1880 | `		if( !bStrict ){` |
|         - | 1881 | `			/*` |
|         - | 1882 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 1883 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 1884 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 1885 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 1886 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 1887 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 1888 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 1889 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 1890 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 1891 | `			 * below, unchanged.` |
|         - | 1892 | `			 */` |
|    305266 | 1893 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 1894 | `				/*` |
|         - | 1895 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 1896 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 1897 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 1898 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 1899 | `				 * really does compare as doubles, so` |
|         - | 1900 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 1901 | `				 *` |
|         - | 1902 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 1903 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 1904 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 1905 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 1906 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 1907 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 1908 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 1909 | `				 *    one, no conversion involved -- which is why` |
|         - | 1910 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 1911 | `				 *    reach the same double.` |
|         - | 1912 | `				 *` |
|         - | 1913 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 1914 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 1915 | `				 */` |
|       429 | 1916 | `				int bBytes = 0;` |
|         - | 1917 | `				{` |
|       429 | 1918 | `					ph7_real r1 = 0, r2 = 0;` |
|       429 | 1919 | `					int iOf1 = 0, iOf2 = 0;` |
|       429 | 1920 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|       429 | 1921 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|       429 | 1922 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 1923 | `						bBytes = 1;` |
|       379 | 1924 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        37 | 1925 | `						return iOf1;` |
|       301 | 1926 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        17 | 1927 | `						return -iOf2;` |
|         - | 1928 | `					}` |
|         - | 1929 | `				}` |
|       385 | 1930 | `				if( !bBytes ){` |
|         - | 1931 | `					/* Perform a numeric comparison */` |
|       285 | 1932 | `					goto Numeric;` |
|         - | 1933 | `				}` |
|        50 | 1934 | `			}` |
|    152352 | 1935 | `		}` |
|         - | 1936 | `		/* Perform a strict string comparison.*/` |
|   1244233 | 1937 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        25 | 1938 | `			PH7_MemObjToString(pObj1);` |
|        12 | 1939 | `		}` |
|   1244233 | 1940 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        35 | 1941 | `			PH7_MemObjToString(pObj2);` |
|        17 | 1942 | `		}` |
|   1244233 | 1943 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   1244233 | 1944 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 1945 | `		/*` |
|         - | 1946 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 1947 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 1948 | `		 */` |
|   1244233 | 1949 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   1244233 | 1950 | `		if( rc == 0 ){` |
|    389434 | 1951 | `			if( s1.nByte != s2.nByte ){` |
|     23502 | 1952 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     11755 | 1953 | `			}` |
|    194721 | 1954 | `		}` |
|   1244233 | 1955 | `		return rc;` |
|    855354 | 1956 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    426806 | 1957 | `Numeric:` |
|         - | 1958 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|    855636 | 1959 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       227 | 1960 | `			PH7_MemObjToNumeric(pObj1);` |
|       110 | 1961 | `		}` |
|    855636 | 1962 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       241 | 1963 | `			PH7_MemObjToNumeric(pObj2);` |
|       117 | 1964 | `		}` |
|    855636 | 1965 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 1966 | `			/*` |
|         - | 1967 | `			 * Symisc eXtension to the PHP language:` |
|         - | 1968 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 1969 | `			 */` |
|         - | 1970 | `			ph7_real r1,r2;` |
|         - | 1971 | `			/* Compare as reals */` |
|       517 | 1972 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        27 | 1973 | `				PH7_MemObjToReal(pObj1);` |
|        13 | 1974 | `			}` |
|       517 | 1975 | `			r1 = pObj1->rVal;` |
|       517 | 1976 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        61 | 1977 | `				PH7_MemObjToReal(pObj2);` |
|        30 | 1978 | `			}` |
|       517 | 1979 | `			r2 = pObj2->rVal;` |
|       517 | 1980 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 1981 | `				/*` |
|         - | 1982 | `				 * Keep a strict three-way comparator contract even for NaN values.` |
|         - | 1983 | `				 * For ordering purposes, NaN compares equal to NaN and greater than` |
|         - | 1984 | `				 * any non-NaN numeric value.` |
|         - | 1985 | `				 */` |
|        52 | 1986 | `				if( PH7_IS_NAN(r1) ){` |
|        42 | 1987 | `					return PH7_IS_NAN(r2) ? 0 : 1;` |
|         - | 1988 | `				}` |
|        11 | 1989 | `				return -1;` |
|         - | 1990 | `			}` |
|       467 | 1991 | `			if( r1 > r2 ){` |
|        77 | 1992 | `				return 1;` |
|       394 | 1993 | `			}else if( r1 < r2 ){` |
|       170 | 1994 | `				return -1;` |
|         - | 1995 | `			}` |
|       226 | 1996 | `			return 0;` |
|       ! 0 | 1997 | `		}else{` |
|         - | 1998 | `			/* Integer comparison */` |
|    855124 | 1999 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|      7265 | 2000 | `				return 1;` |
|    847864 | 2001 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|    839079 | 2002 | `				return -1;` |
|         - | 2003 | `			}` |
|      8790 | 2004 | `			return 0;` |
|         - | 2005 | `		}` |
|         - | 2006 | `	}` |
|         - | 2007 | `	/* NOT REACHED */` |
|       ! 0 | 2008 | `	return 0;` |
|   1237457 | 2009 | `}` |
|         - | 2010 | `/*` |
|         - | 2011 | ` * Perform an addition operation of two ph7_values.` |
|         - | 2012 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 2013 | ` * is that the '+' operator is overloaded.` |
|         - | 2014 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 2015 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 2016 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 2017 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 2018 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 2019 | ` * be ignored.` |
|         - | 2020 | ` * This function take care of handling all the scenarios.` |
|         - | 2021 | ` */` |
|     26410 | 2022 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 2023 | `{` |
|     26415 | 2024 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2025 | `			/* Arithemtic operation */` |
|     21413 | 2026 | `			PH7_MemObjToNumeric(pObj1);` |
|     21413 | 2027 | `			PH7_MemObjToNumeric(pObj2);` |
|     21413 | 2028 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 2029 | `				/* Floating point arithmetic */` |
|         - | 2030 | `				ph7_real a,b;` |
|       118 | 2031 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        34 | 2032 | `					PH7_MemObjToReal(pObj1);` |
|        16 | 2033 | `				}` |
|       118 | 2034 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        53 | 2035 | `					PH7_MemObjToReal(pObj2);` |
|        26 | 2036 | `				}` |
|       118 | 2037 | `				a = pObj1->rVal;` |
|       118 | 2038 | `				b = pObj2->rVal;` |
|       118 | 2039 | `				pObj1->rVal = a+b;` |
|       118 | 2040 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2041 | `				/* Try to get an integer representation also */` |
|       118 | 2042 | `				MemObjTryIntger(&(*pObj1));` |
|        60 | 2043 | `			}else{` |
|         - | 2044 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 2045 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 2046 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 2047 | `				sxi64 a,b,r;` |
|     21297 | 2048 | `				a = pObj1->x.iVal;` |
|     21297 | 2049 | `				b = pObj2->x.iVal;` |
|     21297 | 2050 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 2051 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        11 | 2052 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        11 | 2053 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2054 | `#else` |
|         - | 2055 | `					pObj1->x.iVal = r;` |
|         - | 2056 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2057 | `#endif` |
|         6 | 2058 | `				}else{` |
|     21287 | 2059 | `					pObj1->x.iVal = r;` |
|     21287 | 2060 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2061 | `				}` |
|         - | 2062 | `			}` |
|     10709 | 2063 | `	}else{` |
|      5007 | 2064 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 2065 | `			ph7_hashmap *pMap;` |
|         - | 2066 | `			sxi32 rc;` |
|      5007 | 2067 | `			if( bAddStore ){` |
|         - | 2068 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 2069 | `				 */` |
|         3 | 2070 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2071 | `					/* Force a hashmap cast */` |
|       ! 0 | 2072 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 2073 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2074 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2075 | `						return rc;` |
|         - | 2076 | `					}` |
|       ! 0 | 2077 | `				}` |
|         - | 2078 | `				/* COW separate before in-place mutation */` |
|         3 | 2079 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         2 | 2080 | `			}else{` |
|         - | 2081 | `				/* Create a new hashmap */` |
|      5005 | 2082 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      5005 | 2083 | `				if( pMap == 0){` |
|       ! 0 | 2084 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2085 | `					return SXERR_MEM;` |
|         - | 2086 | `				}` |
|         - | 2087 | `			}` |
|      5007 | 2088 | `			if( !bAddStore ){` |
|      5005 | 2089 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 2090 | `					/* Perform a hashmap duplication */` |
|      5005 | 2091 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2505 | 2092 | `				}else{` |
|       ! 0 | 2093 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2094 | `						/* Simple insertion */` |
|       ! 0 | 2095 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 2096 | `					}` |
|         - | 2097 | `				}` |
|      2500 | 2098 | `			}` |
|         - | 2099 | `			/* Perform the union */` |
|      5007 | 2100 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      5007 | 2101 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2506 | 2102 | `			}else{` |
|       ! 0 | 2103 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2104 | `					/* Simple insertion */` |
|       ! 0 | 2105 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 2106 | `				}` |
|         - | 2107 | `			}` |
|         - | 2108 | `			/* Reflect the change */` |
|      5007 | 2109 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 2110 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 2111 | `			}` |
|      5007 | 2112 | `			pObj1->x.pOther = pMap;` |
|      5007 | 2113 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2501 | 2114 | `		}` |
|         - | 2115 | `	}` |
|     26415 | 2116 | `	return SXRET_OK;` |
|     13210 | 2117 | `}` |
|         - | 2118 | `/*` |
|         - | 2119 | ` * Return a printable representation of the type of a given` |
|         - | 2120 | ` * ph7_value.` |
|         - | 2121 | ` */` |
|         4 | 2122 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|         1 | 2123 | `{` |
|         5 | 2124 | `	const char *zType = "";` |
|         5 | 2125 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 2126 | `		zType = "null";` |
|         5 | 2127 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 2128 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 2129 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 2130 | `		zType = "double";` |
|         5 | 2131 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 2132 | `		zType = "int";` |
|         5 | 2133 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|         3 | 2134 | `		zType = "string";` |
|         4 | 2135 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2136 | `		zType = "bool";` |
|         3 | 2137 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 2138 | `		zType = "array";` |
|         1 | 2139 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 2140 | `		zType = "object";` |
|       ! 0 | 2141 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 2142 | `		zType = "resource";` |
|       ! 0 | 2143 | `	}` |
|         5 | 2144 | `	return zType;` |
|         1 | 2145 | `}` |
|         - | 2146 | `/*` |
|         - | 2147 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 2148 | ` * Store the dump in the given blob.` |
|         - | 2149 | ` */` |
|         - | 2150 | `/*` |
|         - | 2151 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2152 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2153 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2154 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2155 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2156 | ` */` |
|       106 | 2157 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         3 | 2158 | `{` |
|         - | 2159 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2160 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2161 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2162 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2163 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2164 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2165 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2166 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2167 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       109 | 2168 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2169 | `#else` |
|         - | 2170 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2171 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2172 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2173 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2174 | `	}else{` |
|         - | 2175 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2176 | `	}` |
|         - | 2177 | `#endif` |
|       109 | 2178 | `}` |
|         - | 2179 | `/*` |
|         - | 2180 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2181 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2182 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2183 | ` */` |
|       496 | 2184 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         5 | 2185 | `{` |
|       501 | 2186 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|         7 | 2187 | `		return;` |
|         - | 2188 | `	}` |
|       495 | 2189 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2190 | `		if( pObj->x.iVal != 0 ){` |
|       ! 0 | 2191 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       ! 0 | 2192 | `		}` |
|       ! 0 | 2193 | `		return;` |
|         - | 2194 | `	}` |
|       495 | 2195 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2196 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2197 | `		 * non-strings into the output) */` |
|       334 | 2198 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       332 | 2199 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       164 | 2200 | `		}` |
|       334 | 2201 | `		return;` |
|         - | 2202 | `	}` |
|       165 | 2203 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       253 | 2204 | `}` |
|      8678 | 2205 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2206 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2207 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2208 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2209 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2210 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2211 | `	int nDepth,        /* Nesting level */` |
|         - | 2212 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2213 | `	)` |
|         5 | 2214 | `{` |
|      8683 | 2215 | `	sxi32 rc = SXRET_OK;` |
|         - | 2216 | `	int i;` |
|      8683 | 2217 | `	if( !ShowType ){` |
|         - | 2218 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2219 | `		 * containers render the Array/Object block (which the container` |
|         - | 2220 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       241 | 2221 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       181 | 2222 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2223 | `		}` |
|        64 | 2224 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        62 | 2225 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2226 | `		}` |
|         3 | 2227 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         3 | 2228 | `		return SXRET_OK;` |
|         - | 2229 | `	}` |
|         - | 2230 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2231 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2232 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     17339 | 2233 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      8897 | 2234 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      4451 | 2235 | `	}` |
|      8447 | 2236 | `	if( isRef ){` |
|        64 | 2237 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        31 | 2238 | `	}` |
|      8447 | 2239 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       197 | 2240 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       197 | 2241 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2242 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|         7 | 2243 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|         7 | 2244 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|         7 | 2245 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|         7 | 2246 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|         3 | 2247 | `			}` |
|         7 | 2248 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         7 | 2249 | `			return SXRET_OK;` |
|         - | 2250 | `		}` |
|       191 | 2251 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       191 | 2252 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       191 | 2253 | `		return rc;` |
|         - | 2254 | `	}` |
|      8253 | 2255 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       279 | 2256 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       279 | 2257 | `		return SXRET_OK;` |
|         - | 2258 | `	}` |
|      7979 | 2259 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      1013 | 2260 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|      1013 | 2261 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1013 | 2262 | `		return rc;` |
|         - | 2263 | `	}` |
|      6971 | 2264 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      2129 | 2265 | `		if( pObj->x.iVal != 0 ){` |
|      1297 | 2266 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       651 | 2267 | `		}else{` |
|       837 | 2268 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2269 | `		}` |
|      2129 | 2270 | `		return SXRET_OK;` |
|         - | 2271 | `	}` |
|      4847 | 2272 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2273 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2274 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       109 | 2275 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       109 | 2276 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       109 | 2277 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       109 | 2278 | `		return SXRET_OK;` |
|         - | 2279 | `	}` |
|      4741 | 2280 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      2345 | 2281 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      2345 | 2282 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      2345 | 2283 | `		return SXRET_OK;` |
|         - | 2284 | `	}` |
|      2401 | 2285 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      2401 | 2286 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      2401 | 2287 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      2197 | 2288 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      1096 | 2289 | `		}` |
|      2401 | 2290 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      2401 | 2291 | `		return SXRET_OK;` |
|         - | 2292 | `	}` |
|       ! 0 | 2293 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2294 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2295 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2296 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2297 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2298 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2299 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2300 | `		return SXRET_OK;` |
|         - | 2301 | `	}` |
|         - | 2302 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2303 | `	{` |
|       ! 0 | 2304 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2305 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2306 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2307 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2308 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2309 | `	}` |
|       ! 0 | 2310 | `	return rc;` |
|      4344 | 2311 | `}` |
|         - | 2312 |  |
