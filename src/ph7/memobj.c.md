# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1097/1239 lines (88.54%)

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
|      8010 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|      8015 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      7601 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|         - |   69 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|         - |   70 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|         - |   71 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|         - |   72 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|         - |   73 | `	 * A value that IS a float is a float whatever it has cached. */` |
|      7057 |   74 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      6955 |   75 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      4929 |   76 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      1533 |   77 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        35 |   78 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        35 |   79 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   80 | `	return "unknown";` |
|      4010 |   81 | `}` |
|         - |   82 |  |
|         - |   83 | `/*` |
|         - |   84 | ` * Notes on memory objects [i.e: ph7_value].` |
|         - |   85 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |   86 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|         - |   87 | ` * Each ph7_values struct may cache multiple representations (string,` |
|         - |   88 | ` * integer etc.) of the same value.` |
|         - |   89 | ` */` |
|         - |   90 | `/*` |
|         - |   91 | ` * TRUE when a double is what an int64 can hold exactly -- php's` |
|         - |   92 | ` * ZEND_DOUBLE_FITS_LONG with its non-finite screen folded in. The bounds are` |
|         - |   93 | ` * tested in DOUBLE space and the arithmetic there is exact: -2^63 is a double` |
|         - |   94 | ` * to the bit and so is +2^63, one past the range, with no double in between it` |
|         - |   95 | `` * and LARGEST_INT64. Hence `>=` on the way down and `<` on the way up. NaN and`` |
|         - |   96 | ` * both infinities fail one of the two comparisons, so no libm predicate is` |
|         - |   97 | ` * needed to screen them.` |
|         - |   98 | ` */` |
|     23740 |   99 | `PH7_PRIVATE int PH7_RealFitsInt64(double r)` |
|         5 |  100 | `{` |
|     23745 |  101 | `	return r >= -9223372036854775808.0 && r < 9223372036854775808.0;` |
|         5 |  102 | `}` |
|         - |  103 | `/*` |
|         - |  104 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer -- php's` |
|         - |  105 | ` * zend_dval_to_lval, the answer every CAST site gives for a double no int can` |
|         - |  106 | ` * hold: NaN and both infinities are 0, and a finite out-of-range value WRAPS` |
|         - |  107 | `` * modulo 2^64 into the signed band (`(int)1e19` is -8446744073709551616,`` |
|         - |  108 | `` * `(int)1e30` is 5076964154930102272, `(int)1e100` is 0 because every one of`` |
|         - |  109 | ` * its low 64 bits is).` |
|         - |  110 | ` *` |
|         - |  111 | ` * PHL used to answer PHP_INT_MIN for all of them, in silence -- a recorded §2` |
|         - |  112 | ` * divergence, and a silent wrong answer wherever a program casts a computed` |
|         - |  113 | ` * float. The warning php prints beside the value is the cast SITE's to raise:` |
|         - |  114 | ` * this is also the conversion an int representation is speculatively cached` |
|         - |  115 | ` * through (MemObjTryIntger), where php says nothing at all.` |
|         - |  116 | ` *` |
|         - |  117 | ` * php reaches the wrap through fmod(d, 2^64); the same answer comes out of the` |
|         - |  118 | ` * IEEE bits with no libm. A double of magnitude >= 2^63 is already an exact` |
|         - |  119 | ` * integer -- its mantissa is scaled by 2^11 at least -- so the low 64 bits are` |
|         - |  120 | ` * the 53-bit mantissa shifted LEFT, which is 0 once the shift reaches 64.` |
|         - |  121 | ` */` |
|     22578 |  122 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r)` |
|         5 |  123 | `{` |
|         - |  124 | `  union { double d; sxu64 u; } bits;` |
|         - |  125 | `  sxu64 uMag;` |
|         - |  126 | `  int iShift;` |
|     22583 |  127 | `  if( PH7_RealFitsInt64(r) ){` |
|         - |  128 | `    /* In range: php truncates toward zero, and so does C. */` |
|     20991 |  129 | `    return (sxi64)r;` |
|         - |  130 | `  }` |
|      1597 |  131 | `  if( PH7_IS_NAN(r) \|\| PH7_IS_INF(r) ){` |
|       533 |  132 | `    return 0;` |
|         - |  133 | `  }` |
|      1069 |  134 | `  bits.d = r;` |
|         - |  135 | `  /* Unbiased exponent, minus the 52 fraction bits: the power of two the` |
|         - |  136 | `  ** mantissa is scaled by. \|r\| >= 2^63 puts it at 11 or more. */` |
|      1069 |  137 | `  iShift = (int)((bits.u >> 52) & 0x7FF) - 1023 - 52;` |
|      1069 |  138 | `  if( iShift >= 64 ){` |
|         - |  139 | `    /* Every set bit sits above the 64th, so the residue is 0 -- and the shift` |
|         - |  140 | `    ** below would be undefined. */` |
|       173 |  141 | `    return 0;` |
|         - |  142 | `  }` |
|       899 |  143 | `  uMag = ((bits.u & 0x000FFFFFFFFFFFFFULL) \| 0x0010000000000000ULL) << iShift;` |
|       899 |  144 | `  if( bits.u >> 63 ){` |
|         - |  145 | ``     /* Unsigned negation is the two's-complement residue php's `dmod += 2^64` `` |
|         - |  146 | `    ** arrives at, and is defined for every input including 0. */` |
|        51 |  147 | `    uMag = (sxu64)0 - uMag;` |
|        25 |  148 | `  }` |
|       899 |  149 | `  return (sxi64)uMag;` |
|     11294 |  150 | `}` |
|     22072 |  151 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |  152 | `{` |
|         - |  153 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  154 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |  155 | `	 * is omitted from the build.` |
|         - |  156 | `	 */` |
|         - |  157 | `	return pObj->rVal;` |
|         - |  158 | `#else` |
|     22077 |  159 | `	return PH7_RealToInt64(pObj->rVal);` |
|         - |  160 | `#endif` |
|         5 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | `` * php's `Warning: The float %s is not representable as an int, cast occurred`,`` |
|         - |  164 | ` * printed BESIDE the wrapped value MemObjRealToInt answers -- at every CAST` |
|         - |  165 | `` * site, which is what php's zend_dval_to_lval raises it from: `(int)$f`,`` |
|         - |  166 | `` * `intval()`, `settype()`, the printf integer conversions, and a native`` |
|         - |  167 | ` * subscript that reads an int out of its offset.` |
|         - |  168 | ` *` |
|         - |  169 | `` * Not a DEPRECATION: php's other float->int diagnostic (`Implicit conversion`` |
|         - |  170 | `` * from float %s to int loses precision`) is the E_DEPRECATED that §10 refuses`` |
|         - |  171 | ` * outright with a TypeError, and it fires at the sites this one does NOT --` |
|         - |  172 | ` * the operators, the array key, the int parameter, none of which reach a cast` |
|         - |  173 | ` * here because the refusal comes first. An explicit cast is never lossy in` |
|         - |  174 | ` * php's eyes, so this warning is all it says. The two other conversions that` |
|         - |  175 | ` * read an int out of a float say nothing at all and must not call this: the` |
|         - |  176 | ` * speculative int representation (MemObjTryIntger) and php's string-offset` |
|         - |  177 | ` * cast, which has a message of its own.` |
|         - |  178 | ` *` |
|         - |  179 | `` * The value is rendered the way php's `%.*H` renders it -- the shortest`` |
|         - |  180 | ` * decimal that round-trips, the shape var_dump and serialize already share.` |
|         - |  181 | ` */` |
|       828 |  182 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r)` |
|         4 |  183 | `{` |
|         - |  184 | `	SyBlob sVal;` |
|         - |  185 | `	char zVal[64];` |
|       832 |  186 | `	if( pVm == 0 \|\| PH7_RealFitsInt64(r) ){` |
|       616 |  187 | `		return;` |
|         - |  188 | `	}` |
|       219 |  189 | `	SyBlobInitFromBuf(&sVal,zVal,(sxu32)sizeof(zVal) - 1);` |
|       219 |  190 | `	PH7_AppendShortestReal(&sVal,r);` |
|       219 |  191 | `	zVal[SyBlobLength(&sVal)] = 0;   /* the blob is LOCKED: it truncates, never grows */` |
|       327 |  192 | `	VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       108 |  193 | `		"The float %s is not representable as an int, cast occurred",zVal);` |
|       418 |  194 | `}` |
|         - |  195 | `/* The same warning asked of a VALUE: only a float can carry one, and the flag` |
|         - |  196 | ` * test mirrors the conversion's own routing (MemObjIntValue reads MEMOBJ_REAL` |
|         - |  197 | ` * first), so the diagnostic and the answer always describe the same branch. */` |
|   1177870 |  198 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj)` |
|         5 |  199 | `{` |
|   1177875 |  200 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|   1177537 |  201 | `		return;` |
|         - |  202 | `	}` |
|       342 |  203 | `	PH7_RealWarnIntCast(pObj->pVm,(double)pObj->rVal);` |
|    588937 |  204 | `}` |
|         - |  205 | `/*` |
|         - |  206 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  207 | ` * to a 64-bit integer.` |
|         - |  208 | ` */` |
|    693784 |  209 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  210 | `{` |
|    693789 |  211 | `	sxi64 iVal = 0;` |
|    693789 |  212 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  213 | `		return 0;` |
|         - |  214 | `	}` |
|    693789 |  215 | `	if( pVal->zString[0] == '0' ){` |
|         - |  216 | `		sxi32 c;` |
|    277153 |  217 | `		if( pVal->nByte == sizeof(char) ){` |
|    270819 |  218 | `			return 0;` |
|         - |  219 | `		}` |
|      6339 |  220 | `		c = pVal->zString[1];` |
|      6339 |  221 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  222 | `			/* Hex digit stream */` |
|       186 |  223 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      6248 |  224 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  225 | `			/* Binary digit stream */` |
|       285 |  226 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      6015 |  227 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  228 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  229 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  230 | `			if( pVal->nByte > 2 ){` |
|        21 |  231 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  232 | `			}` |
|        11 |  233 | `		}else{` |
|         - |  234 | `			/* Legacy octal digit stream (leading 0) */` |
|      5853 |  235 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  236 | `		}` |
|      3172 |  237 | `	}else{` |
|         - |  238 | `		/* Decimal digit stream */` |
|    416641 |  239 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  240 | `	}` |
|    422975 |  241 | `	return iVal;` |
|    346897 |  242 | `}` |
|         - |  243 | `/*` |
|         - |  244 | ` * TRUE when the numeric PREFIX that ends at zTail is float-SHAPED -- it carries` |
|         - |  245 | ` * a '.' or a complete exponent. This is php's is_numeric_string answering` |
|         - |  246 | ` * IS_DOUBLE, and it decides which of two entirely different readings the bytes` |
|         - |  247 | ` * get: an integer-shaped run is read from its DIGITS, a float-shaped one from` |
|         - |  248 | ` * the double they spell.` |
|         - |  249 | ` */` |
|   1176686 |  250 | `static int MemObjNumericPrefixIsFloat(ph7_value *pObj,const char *zTail)` |
|         5 |  251 | `{` |
|   1176691 |  252 | `	const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|   3928679 |  253 | `	while( z < zTail ){` |
|   2752281 |  254 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       291 |  255 | `			return TRUE;` |
|         - |  256 | `		}` |
|   2751993 |  257 | `		z++;` |
|         5 |  258 | `	}` |
|   1176403 |  259 | `	return FALSE;` |
|    588336 |  260 | `}` |
|         - |  261 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  262 | `/*` |
|         - |  263 | ` * php's zend_dval_to_lval_cap: the double->int conversion a NUMERIC STRING` |
|         - |  264 | ` * takes, which is not the one a real float takes. This one SATURATES at the` |
|         - |  265 | ` * int64 bounds and answers 0 for a value that is not finite, where the cast of` |
|         - |  266 | ` * an actual float answers PHP_INT_MIN for every out-of-range case` |
|         - |  267 | ` * (MemObjRealToInt -- a recorded divergence). PHL has always` |
|         - |  268 | `` * saturated the integer-shaped overflow, so `(int)"99999999999999999999"` is`` |
|         - |  269 | ` * PHP_INT_MAX in both engines; this is the same rule for the shapes that reach` |
|         - |  270 | ` * it through a double.` |
|         - |  271 | ` */` |
|       208 |  272 | `static sxi64 MemObjRealToIntCap(ph7_real r)` |
|         2 |  273 | `{` |
|         - |  274 | `	/* NaN fails both comparisons and either infinity fails one of them, so this` |
|         - |  275 | `	 * screens all three without a libm predicate. */` |
|       210 |  276 | `	if( !(r >= -1.7976931348623157e308 && r <= 1.7976931348623157e308) ){` |
|        16 |  277 | `		return 0;` |
|         - |  278 | `	}` |
|       196 |  279 | `	if( r >= 9223372036854775808.0 ){    /* +2^63, exact in double space */` |
|        24 |  280 | `		return LARGEST_INT64;` |
|         - |  281 | `	}` |
|       174 |  282 | `	if( r < -9223372036854775808.0 ){` |
|         5 |  283 | `		return SMALLEST_INT64;` |
|         - |  284 | `	}` |
|       170 |  285 | `	return (sxi64)r;` |
|       106 |  286 | `}` |
|         - |  287 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  288 | `/*` |
|         - |  289 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  290 | ` * do at representing the value that pObj describes as a string` |
|         - |  291 | ` * representation.` |
|         - |  292 | ` */` |
|   1175956 |  293 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  294 | `{` |
|   1175961 |  295 | `	sxi64 iVal = 0;` |
|         - |  296 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   1175961 |  297 | `	const char *zTail = 0;` |
|   1175956 |  298 | `	if( PH7_MemObjStringNumericPrefix(pObj,&zTail)` |
|   1175951 |  299 | `	 && MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|         - |  300 | `		/* A float-shaped string is a DOUBLE first and an int second, which is the` |
|         - |  301 | ``		 * only reading that makes `(int)"1e3"` the 1000 it says: reading its`` |
|         - |  302 | `		 * digits stops at the 'e' and answers the mantissa's integer part, so` |
|         - |  303 | `		 * "1e3" was 1, "1.5e2" was 1 and "-2e2" was -2. The '.' forms were wrong` |
|         - |  304 | `		 * the same way wherever the double rounds away from the digits --` |
|         - |  305 | ``		 * `(int)"0.9999999999999999999"` is 1, not 0. */`` |
|       210 |  306 | `		ph7_real rVal = 0.0;` |
|       210 |  307 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),` |
|         - |  308 | `			(void *)&rVal,0);` |
|       210 |  309 | `		if( pOverflow ){` |
|         - |  310 | `			/* php reports no overflow for a float-shaped string however large it` |
|         - |  311 | `			 * is: it was always going to be a double, so no digits were lost. */` |
|       ! 0 |  312 | `			*pOverflow = 0;` |
|       ! 0 |  313 | `		}` |
|       210 |  314 | `		return MemObjRealToIntCap(rVal);` |
|         - |  315 | `	}` |
|         - |  316 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  317 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  318 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  319 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|   1175753 |  320 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|   1175753 |  321 | `	return iVal;` |
|    587975 |  322 | `}` |
|         - |  323 | `/*` |
|         - |  324 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  325 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  326 | ` * successfully called. Any other return value indicates failure.` |
|         - |  327 | ` */` |
|      1832 |  328 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  329 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  330 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  331 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  332 | `	sxu32 nLen,                /* Method name length */` |
|         - |  333 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  334 | `	)` |
|         5 |  335 | `{` |
|         - |  336 | `	ph7_class_method *pMethod;` |
|         - |  337 | `	/* Check if the method is available */` |
|      1837 |  338 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      1837 |  339 | `	if( pMethod == 0 ){` |
|         - |  340 | `		/* No such method */` |
|         5 |  341 | `		return SXERR_NOTFOUND;` |
|         - |  342 | `	}` |
|         - |  343 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  344 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  345 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  346 | `	 * printed "Object" after a caught __toString() throw). */` |
|      1833 |  347 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       921 |  348 | `}` |
|         - |  349 | `/*` |
|         - |  350 | ` * Return some kind of integer value which is the best we can` |
|         - |  351 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  352 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  353 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  354 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  355 | ` * a integer and return that.` |
|         - |  356 | ` * If pObj represents a NULL value, return 0.` |
|         - |  357 | ` */` |
|   1176372 |  358 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  359 | `{` |
|         - |  360 | `	sxi32 iFlags;` |
|   1176377 |  361 | `	iFlags = pObj->iFlags;` |
|   1176377 |  362 | `	if (iFlags & MEMOBJ_REAL ){` |
|       356 |  363 | `		return MemObjRealToInt(&(*pObj));` |
|   1176025 |  364 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      1887 |  365 | `		return pObj->x.iVal;` |
|   1174143 |  366 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  367 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  368 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  369 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|   1173513 |  370 | `		return MemObjStringToInt(&(*pObj),0);` |
|       634 |  371 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       589 |  372 | `		return 0;` |
|        46 |  373 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  374 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  375 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  376 | `		 * followed php; int/float did not.) */` |
|        27 |  377 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        27 |  378 | `		sxu32 n = pMap->nEntry;` |
|        27 |  379 | `		PH7_HashmapUnref(pMap);` |
|        27 |  380 | `		return n > 0 ? 1 : 0;` |
|        20 |  381 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  382 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  383 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  384 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses. */`` |
|         9 |  385 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         9 |  386 | `		if( pInst && pInst->pClass ){` |
|        13 |  387 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         8 |  388 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|         4 |  389 | `		}` |
|         9 |  390 | `		PH7_ClassInstanceUnref(pInst);` |
|         9 |  391 | `		return 1;` |
|        12 |  392 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  393 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  394 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        12 |  395 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  396 | `	}` |
|         - |  397 | `	/* CANT HAPPEN */` |
|       ! 0 |  398 | `	return 0;` |
|    588191 |  399 | `}` |
|         - |  400 | `/*` |
|         - |  401 | ` * Return some kind of real value which is the best we can` |
|         - |  402 | ` * do at representing the value that pObj describes as a real.` |
|         - |  403 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  404 | ` * integer then the integer  is promoted to real and that value` |
|         - |  405 | ` * is returned.` |
|         - |  406 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  407 | ` * into a real and return that.` |
|         - |  408 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  409 | ` */` |
|     18046 |  410 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  411 | `{` |
|         - |  412 | `	sxi32 iFlags;` |
|     18051 |  413 | `	iFlags = pObj->iFlags;` |
|     18051 |  414 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  415 | `		return pObj->rVal;` |
|     18051 |  416 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3661 |  417 | `		return (ph7_real)pObj->x.iVal;` |
|     14395 |  418 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  419 | `		SyString sString;` |
|         - |  420 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  421 | `		ph7_real rVal = 0;` |
|         - |  422 | `#else` |
|     14375 |  423 | `		ph7_real rVal = 0.0;` |
|         - |  424 | `#endif` |
|     14375 |  425 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     14375 |  426 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  427 | `			/* Convert as much as we can */` |
|         - |  428 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  429 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  430 | `#else` |
|     14371 |  431 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  432 | `#endif` |
|      7183 |  433 | `		}` |
|     14375 |  434 | `		return rVal;` |
|        22 |  435 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  436 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  437 | `		return 0;` |
|         - |  438 | `#else` |
|         9 |  439 | `		return 0.0;` |
|         - |  440 | `#endif` |
|        14 |  441 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  442 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  443 | `		 * branch above. */` |
|       ! 0 |  444 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  445 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  446 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  447 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        14 |  448 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  449 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        12 |  450 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        12 |  451 | `		if( pInst && pInst->pClass ){` |
|        17 |  452 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        10 |  453 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|         5 |  454 | `		}` |
|        12 |  455 | `		PH7_ClassInstanceUnref(pInst);` |
|        12 |  456 | `		return (ph7_real)1.0;` |
|         3 |  457 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  458 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  459 | `	}` |
|         - |  460 | `	/* NOT REACHED  */` |
|       ! 0 |  461 | `	return 0;` |
|      9028 |  462 | `}` |
|         - |  463 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  464 | `/*` |
|         - |  465 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  466 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  467 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  468 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  469 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  470 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  471 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  472 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  473 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  474 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  475 | ` */` |
|       848 |  476 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  477 | `{` |
|         - |  478 | `	sxi32 iExp,i;` |
|       853 |  479 | `	iExp = nLen - 1;` |
|      6533 |  480 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      5685 |  481 | `		iExp--;` |
|         5 |  482 | `	}` |
|       853 |  483 | `	if( iExp <= 0 ){` |
|       737 |  484 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  485 | `	}` |
|         - |  486 | `	{` |
|       118 |  487 | `		sxi32 iDig = iExp + 1;` |
|         - |  488 | `		sxi32 iFirst;` |
|       118 |  489 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       118 |  490 | `			iDig++;` |
|        58 |  491 | `		}` |
|       118 |  492 | `		iFirst = iDig;` |
|       156 |  493 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       100 |  494 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        27 |  495 | `			iFirst++;` |
|         1 |  496 | `		}` |
|       118 |  497 | `		if( iFirst > iDig ){` |
|        27 |  498 | `			sxi32 nStrip = iFirst - iDig;` |
|        79 |  499 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        53 |  500 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        27 |  501 | `			}` |
|        27 |  502 | `			nLen -= nStrip;` |
|        13 |  503 | `		}` |
|         - |  504 | `	}` |
|       118 |  505 | `	if( bGeneric ){` |
|       102 |  506 | `		int bHasDot = 0;` |
|       226 |  507 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       190 |  508 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        64 |  509 | `		}` |
|       102 |  510 | `		if( !bHasDot ){` |
|       220 |  511 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       184 |  512 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        93 |  513 | `			}` |
|        38 |  514 | `			zBuf[iExp] = '.';` |
|        38 |  515 | `			zBuf[iExp+1] = '0';` |
|        38 |  516 | `			nLen += 2;` |
|        18 |  517 | `		}` |
|        50 |  518 | `	}` |
|       118 |  519 | `	return nLen;` |
|       429 |  520 | `}` |
|         - |  521 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  522 | `/*` |
|         - |  523 | ` * Return the string representation of a given ph7_value.` |
|         - |  524 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  525 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  526 | ` * left without a rendering of pObj.` |
|         - |  527 | ` */` |
|     81410 |  528 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  529 | `{` |
|     81415 |  530 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  531 | `		/* Handle special floating-point values first */` |
|       561 |  532 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|        25 |  533 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       549 |  534 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        11 |  535 | `			if( pObj->rVal < 0.0 ){` |
|         3 |  536 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|         2 |  537 | `			}else{` |
|         9 |  538 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  539 | `			}` |
|         6 |  540 | `		}else{` |
|         - |  541 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  542 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  543 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  544 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  545 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  546 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  547 | `			 * exponent/fraction quirks. */` |
|         - |  548 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       527 |  549 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       527 |  550 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  551 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  552 | `			}` |
|       527 |  553 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       527 |  554 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  555 | `#else` |
|         - |  556 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  557 | `#endif` |
|         5 |  558 | `		}` |
|     81137 |  559 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     77963 |  560 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  561 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     41872 |  562 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       441 |  563 | `		if( bStrictBool ){` |
|         - |  564 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       441 |  565 | `			if( pObj->x.iVal ){` |
|       223 |  566 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       109 |  567 | `			}` |
|         - |  568 | `			/* false produces empty string, nothing to append */` |
|       223 |  569 | `		}else{` |
|         - |  570 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  571 | `			if( pObj->x.iVal ){` |
|       ! 0 |  572 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  573 | `			}else{` |
|       ! 0 |  574 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  575 | `			}` |
|         5 |  576 | `		}` |
|      2683 |  577 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       238 |  578 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       238 |  579 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      2348 |  580 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  581 | `		ph7_value sResult;` |
|         - |  582 | `		sxi32 rc;` |
|         - |  583 | `		/* Invoke the __toString() method if available */` |
|      1837 |  584 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      1837 |  585 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  586 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      1837 |  587 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  588 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  589 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  590 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  591 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  592 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  593 | `			 * instance reference. */` |
|       197 |  594 | `			PH7_MemObjRelease(&sResult);` |
|       197 |  595 | `			return rc;` |
|         - |  596 | `		}` |
|      1645 |  597 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  598 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  599 | `			 * value, and requiring a non-empty one sent` |
|         - |  600 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  601 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  602 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  603 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  604 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  605 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  606 | `			 * result no return-type check governs. */` |
|      1641 |  607 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       823 |  608 | `		}else{` |
|         - |  609 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  610 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         5 |  611 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  612 | `		}` |
|      1645 |  613 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      1645 |  614 | `		PH7_MemObjRelease(&sResult);` |
|      1219 |  615 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  616 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  617 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  618 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  619 | `	}` |
|     81223 |  620 | `	return SXRET_OK;` |
|     40702 |  621 | `}` |
|         - |  622 | `/*` |
|         - |  623 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  624 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  625 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  626 | ` * (php's exact set):` |
|         - |  627 | ` * NULL` |
|         - |  628 | ` * the boolean FALSE itself.` |
|         - |  629 | ` * the integer 0 (zero).` |
|         - |  630 | ` * the real 0.0 (zero).` |
|         - |  631 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  632 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  633 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  634 | ` * and were removed under the §10 PH7-ism policy).` |
|         - |  635 | ` * an array with zero elements.` |
|         - |  636 | ` */` |
|    127270 |  637 | `static sxi32 MemObjIsTruthy(ph7_value *pObj)` |
|         5 |  638 | `{` |
|         - |  639 | `	sxi32 iFlags;` |
|    127275 |  640 | `	iFlags = pObj->iFlags;` |
|    127275 |  641 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  642 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  643 | `		return pObj->rVal ? 1 : 0;` |
|         - |  644 | `#else` |
|         - |  645 | `		/* A NaN is neither zero nor equal to itself, so it is TRUE -- php's` |
|         - |  646 | `		 * answer too, behind the warning PH7_MemObjToBool raises. */` |
|       101 |  647 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  648 | `#endif` |
|    127177 |  649 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         - |  650 | ``		/* BOOL is here for `empty()`, which asks this of a value of ANY type; the`` |
|         - |  651 | `		 * bool CONVERSION never does (it returns early when the bit is set). */` |
|      2783 |  652 | `		return pObj->x.iVal ? 1 : 0;` |
|    124399 |  653 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  654 | `		SyString sString;` |
|     34363 |  655 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  656 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|     34363 |  657 | `		if( sString.nByte == 0 ){` |
|     27001 |  658 | `			return 0;` |
|         - |  659 | `		}` |
|      7367 |  660 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        24 |  661 | `			return 0;` |
|         - |  662 | `		}` |
|      7345 |  663 | `		return 1;` |
|     90041 |  664 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     70607 |  665 | `		return 0;` |
|     19439 |  666 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     17523 |  667 | `		return ((ph7_hashmap *)pObj->x.pOther)->nEntry > 0 ? TRUE : FALSE;` |
|      1921 |  668 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  669 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  670 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  671 | `		 * extension changed control flow in valid php source.` |
|         - |  672 | `		 *` |
|         - |  673 | `		 * An INTERNAL class may still install php's cast_object handler for` |
|         - |  674 | `		 * _IS_BOOL, which is a different thing entirely -- it is not reachable` |
|         - |  675 | `		 * from PHP source and php ships exactly one: a zero BcMath\Number is` |
|         - |  676 | ``		 * falsy, so `if ($n)` and `empty($n)` read the VALUE. */`` |
|       289 |  677 | `		int bNative = 1;` |
|       289 |  678 | `		if( PH7_ClassNativeBool((ph7_class_instance *)pObj->x.pOther,&bNative) ){` |
|         5 |  679 | `			return bNative;` |
|         - |  680 | `		}` |
|       285 |  681 | `		return 1;` |
|      1635 |  682 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1635 |  683 | `		return pObj->x.pOther != 0;` |
|         - |  684 | `	}` |
|         - |  685 | `	/* NOT REACHED */` |
|       ! 0 |  686 | `	return 0;` |
|     63640 |  687 | `}` |
|         - |  688 | `/*` |
|         - |  689 | ` * The same question asked by a CONVERSION, which is about to overwrite the` |
|         - |  690 | ` * payload and so owes it a reference drop. Nothing else about the answer` |
|         - |  691 | `` * differs -- which is the point: `empty()` and `array_filter()`'s default test`` |
|         - |  692 | ` * used to carry a SECOND set of rules (PH7_MemObjIsEmpty), and it disagreed.` |
|         - |  693 | ` */` |
|     75556 |  694 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  695 | `{` |
|     75561 |  696 | `	sxi32 rc = MemObjIsTruthy(&(*pObj));` |
|     75561 |  697 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        44 |  698 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|     75540 |  699 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       279 |  700 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       138 |  701 | `	}` |
|     75561 |  702 | `	return rc;` |
|         5 |  703 | `}` |
|         - |  704 | `/*` |
|         - |  705 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  706 | ` */` |
|     21720 |  707 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  708 | `{` |
|     21725 |  709 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  710 | `  /* Only mark the value as an integer if` |
|         - |  711 | `  **` |
|         - |  712 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  713 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  714 | `  **        possible integer` |
|         - |  715 | `  **` |
|         - |  716 | `  ** The second and third terms in the following conditional enforces` |
|         - |  717 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  718 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  719 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  720 | `  ** architectures might behave differently.` |
|         - |  721 | `  */` |
|     21720 |  722 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     16897 |  723 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     16871 |  724 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      8435 |  725 | `	}` |
|     21725 |  726 | `	return SXRET_OK;` |
|         5 |  727 | `}` |
|         - |  728 | `/*` |
|         - |  729 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  730 | ` */` |
|   4144587 |  731 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  732 | `{` |
|   4144592 |  733 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  734 | `		/* Preform the conversion */` |
|   1176377 |  735 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  736 | `		/* Invalidate any prior representations */` |
|   1176377 |  737 | `		SyBlobRelease(&pObj->sBlob);` |
|   1176377 |  738 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|    588186 |  739 | `	}` |
|   4144592 |  740 | `	return SXRET_OK;` |
|         5 |  741 | `}` |
|         - |  742 | `/*` |
|         - |  743 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  744 | ` * Invalidate any prior representations` |
|         - |  745 | ` */` |
|     21552 |  746 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  747 | `{` |
|     21557 |  748 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  749 | `		/* Preform the conversion */` |
|     18051 |  750 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  751 | `		/* Invalidate any prior representations */` |
|     18051 |  752 | `		SyBlobRelease(&pObj->sBlob);` |
|     18051 |  753 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  754 | `		/* Try to get an integer representation */` |
|     18051 |  755 | `		MemObjTryIntger(&(*pObj));` |
|      9023 |  756 | `	}` |
|     21557 |  757 | `	return SXRET_OK;` |
|         5 |  758 | `}` |
|         - |  759 | `/*` |
|         - |  760 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  761 | ` */` |
|    115996 |  762 | `static sxi32 MemObjToBoolQuiet(ph7_value *pObj)` |
|         5 |  763 | `{` |
|    116001 |  764 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  765 | `		/* Preform the conversion */` |
|     75561 |  766 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  767 | `		/* Invalidate any prior representations */` |
|     75561 |  768 | `		SyBlobRelease(&pObj->sBlob);` |
|     75561 |  769 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     37778 |  770 | `	}` |
|    116001 |  771 | `	return SXRET_OK;` |
|         5 |  772 | `}` |
|         - |  773 | `/*` |
|         - |  774 | ` * The same conversion where a php PROGRAM asked for it, which is every` |
|         - |  775 | `` * truthiness site there is: `(bool)`, `if`, `!`, `&&`, the ternary, `empty()`,`` |
|         - |  776 | `` * `boolval()`, `settype()`, a `bool` parameter internal or userland,`` |
|         - |  777 | `` * `array_filter`'s default test. php 8.5 warns from all of them when the value`` |
|         - |  778 | `` * is a NaN -- `unexpected NAN value was coerced to bool` -- and answers TRUE.`` |
|         - |  779 | ` *` |
|         - |  780 | `` * A COMPARISON is not one of them: `NAN == true` is silent in php, and it`` |
|         - |  781 | ` * reaches the same conversion, which is why the quiet form above exists.` |
|         - |  782 | ` */` |
|     43980 |  783 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  784 | `{` |
|     43980 |  785 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     22026 |  786 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        21 |  787 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - |  788 | `			"unexpected NAN value was coerced to bool");` |
|        10 |  789 | `	}` |
|     43985 |  790 | `	return MemObjToBoolQuiet(&(*pObj));` |
|         5 |  791 | `}` |
|         - |  792 | `/*` |
|         - |  793 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  794 | ` */` |
|   8020057 |  795 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  796 | `{` |
|   8020062 |  797 | `	sxi32 rc = SXRET_OK;` |
|   8020062 |  798 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  799 | `		/* Perform the conversion */` |
|     81099 |  800 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     81099 |  801 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     81099 |  802 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  803 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  804 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  805 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  806 | `			 * leaves $o holding the object after the throw is caught. */` |
|       197 |  807 | `			return rc;` |
|         - |  808 | `		}` |
|     80907 |  809 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     40443 |  810 | `	}` |
|   8019870 |  811 | `	return rc;` |
|   4013185 |  812 | `}` |
|         - |  813 | `/*` |
|         - |  814 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  815 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  816 | ` *   Error: Object of class X could not be converted to string` |
|         - |  817 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  818 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  819 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  820 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  821 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  822 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  823 | ` * cast still carried the placeholder.` |
|         - |  824 | ` *` |
|         - |  825 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  826 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - |  827 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - |  828 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - |  829 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - |  830 | ` * settype() site then blanks its target itself, because php's` |
|         - |  831 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - |  832 | ` */` |
|       580 |  833 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         5 |  834 | `{` |
|       585 |  835 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - |  836 | `	SyBlob sMsg;` |
|       585 |  837 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       585 |  838 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       580 |  839 | `		&pInst->pClass->sName);` |
|         - |  840 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       585 |  841 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         5 |  842 | `}` |
|         - |  843 | `/*` |
|         - |  844 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - |  845 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - |  846 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - |  847 | ` */` |
|    103228 |  848 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 |  849 | `{` |
|         - |  850 | `	ph7_class_instance *pInst;` |
|    103233 |  851 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|    100671 |  852 | `		return FALSE;` |
|         - |  853 | `	}` |
|      2567 |  854 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      2567 |  855 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 |  856 | `		return FALSE;` |
|         - |  857 | `	}` |
|      2567 |  858 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     51606 |  859 | `}` |
|         - |  860 | `/*` |
|         - |  861 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  862 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  863 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  864 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  865 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  866 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  867 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  868 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  869 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  870 | ` *` |
|         - |  871 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  872 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - |  873 | ` * ph7_value carries.` |
|         - |  874 | ` *` |
|         - |  875 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - |  876 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - |  877 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - |  878 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - |  879 | ` * exactly as php never throws for them.` |
|         - |  880 | ` *` |
|         - |  881 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - |  882 | ` */` |
|   1264917 |  883 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  884 | `{` |
|   1264922 |  885 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|   1188324 |  886 | `		return SXRET_OK;` |
|         - |  887 | `	}` |
|     76603 |  888 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|       207 |  889 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|       102 |  890 | `	}` |
|         - |  891 | `	/* php 8.5's other coercion warning, and it rides HERE for the same reason` |
|         - |  892 | `	 * that one does: this is the conversion a program asked for -- a cast, echo,` |
|         - |  893 | ``	 * concatenation, interpolation, a `string` parameter -- and not the internal`` |
|         - |  894 | `	 * one a comparison or a debug renderer makes. A NaN is the only float that` |
|         - |  895 | `	 * warns; INF and -INF spell themselves out in silence. */` |
|     76603 |  896 | `	if( (pObj->iFlags & MEMOBJ_REAL) && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        17 |  897 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,` |
|         - |  898 | `			"unexpected NAN value was coerced to string");` |
|         8 |  899 | `	}` |
|     76603 |  900 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       585 |  901 | `		return MemObjThrowNotStringable(pObj);` |
|         - |  902 | `	}` |
|     76023 |  903 | `	return PH7_MemObjToString(pObj);` |
|    632802 |  904 | `}` |
|         - |  905 | `/*` |
|         - |  906 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  907 | ` * representation.` |
|         - |  908 | ` */` |
|         2 |  909 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  910 | `{` |
|         3 |  911 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  912 | `}` |
|         - |  913 | `/*` |
|         - |  914 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  915 | `  * According to the PHP language reference manual.` |
|         - |  916 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  917 | `  *   to an array results in an array with a single element with index zero` |
|         - |  918 | `  *   and the value of the scalar which was converted.` |
|         - |  919 | `  */` |
|      7114 |  920 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  921 | `{` |
|      7119 |  922 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  923 | `		ph7_hashmap *pMap;` |
|         - |  924 | `		/* Allocate a new hashmap instance */` |
|      6511 |  925 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      6511 |  926 | `		if( pMap == 0 ){` |
|       ! 0 |  927 | `			return SXERR_MEM;` |
|         - |  928 | `		}` |
|      6511 |  929 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  930 | `			/*` |
|         - |  931 | `			 * According to the PHP language reference manual.` |
|         - |  932 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  933 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  934 | `			 *   and the value of the scalar which was converted.` |
|         - |  935 | `			 */` |
|       595 |  936 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       567 |  937 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       562 |  938 | `				if( pInst && pObj->pVm->pClosureClass` |
|       567 |  939 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|         - |  940 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|         - |  941 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|         - |  942 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|         - |  943 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|         - |  944 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|         3 |  945 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         2 |  946 | `				}else{` |
|         - |  947 | `					/* Object cast */` |
|       565 |  948 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|         - |  949 | `				}` |
|       286 |  950 | `			}else{` |
|         - |  951 | `				/* Insert a single element */` |
|        30 |  952 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  953 | `			}` |
|       595 |  954 | `			SyBlobRelease(&pObj->sBlob);` |
|       295 |  955 | `		}` |
|         - |  956 | `		/* Invalidate any prior representation */` |
|      6511 |  957 | `		PH7_MemObjRelease(pObj);` |
|      6511 |  958 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      6511 |  959 | `		pObj->x.pOther = pMap;` |
|      3253 |  960 | `	}` |
|      7119 |  961 | `	return SXRET_OK;` |
|      3562 |  962 | `}` |
|         - |  963 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  964 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  965 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  966 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       168 |  967 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         4 |  968 | `{` |
|       172 |  969 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  970 | `	ph7_value *pSlot;` |
|         - |  971 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  972 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  973 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  974 | `	 * safe to coerce in place. */` |
|       172 |  975 | `	PH7_MemObjToString(pKey);` |
|       256 |  976 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       168 |  977 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       172 |  978 | `	if( pSlot ){` |
|       172 |  979 | `		PH7_MemObjStore(pValue,pSlot);` |
|        84 |  980 | `	}` |
|       172 |  981 | `	return SXRET_OK;` |
|         4 |  982 | `}` |
|         - |  983 | `/*` |
|         - |  984 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  985 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  986 | ` * matching PHP's (object) cast:` |
|         - |  987 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  988 | ` *   - scalar -> a single property named "scalar".` |
|         - |  989 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  990 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  991 | ` */` |
|       106 |  992 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         4 |  993 | `{` |
|       110 |  994 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  995 | `		ph7_class_instance *pStd;` |
|         - |  996 | `		ph7_class *pClass;` |
|         - |  997 | `		ph7_vm *pVm;` |
|         - |  998 | `		/* Point to the underlying VM + the stdClass */` |
|       110 |  999 | `		pVm = pObj->pVm;` |
|       163 | 1000 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        53 | 1001 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       110 | 1002 | `		if( pClass == 0 ){` |
|         - | 1003 | `			/* Can't happen,load null instead */` |
|       ! 0 | 1004 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 | 1005 | `			return SXRET_OK;` |
|         - | 1006 | `		}` |
|         - | 1007 | `		/* Instanciate a new (empty) stdClass object */` |
|       110 | 1008 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       110 | 1009 | `		if( pStd == 0 ){` |
|         - | 1010 | `			/* Out of memory */` |
|       ! 0 | 1011 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 | 1012 | `			return SXRET_OK;` |
|         - | 1013 | `		}` |
|       110 | 1014 | `		pStd->iRef = 1;` |
|       110 | 1015 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1016 | `			/* Array: one dynamic property per entry. */` |
|         - | 1017 | `			struct VmObjCastData sData;` |
|        96 | 1018 | `			sData.pVm = pVm;` |
|        96 | 1019 | `			sData.pStd = pStd;` |
|        96 | 1020 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        62 | 1021 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1022 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 | 1023 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 | 1024 | `			if( pSlot ){` |
|        14 | 1025 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 | 1026 | `			}` |
|         6 | 1027 | `		}` |
|         - | 1028 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - | 1029 | `		/* Invalidate any prior representation */` |
|       110 | 1030 | `		PH7_MemObjRelease(pObj);` |
|         - | 1031 | `		/* Save the new instance */` |
|       110 | 1032 | `		pObj->x.pOther = pStd;` |
|       110 | 1033 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        53 | 1034 | `	}` |
|       110 | 1035 | `	return SXRET_OK;` |
|        57 | 1036 | `}` |
|         - | 1037 | `/*` |
|         - | 1038 | ` * Return a pointer to the appropriate convertion method associated` |
|         - | 1039 | ` * with the given type.` |
|         - | 1040 | ` * Note on type juggling.` |
|         - | 1041 | ` * Accoding to the PHP language reference manual` |
|         - | 1042 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - | 1043 | ` *  declaration; a variable's type is determined by the context in which` |
|         - | 1044 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - | 1045 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - | 1046 | ` *  assigned to $var, it becomes an integer.` |
|         - | 1047 | ` */` |
|    100270 | 1048 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 | 1049 | `{` |
|    100275 | 1050 | `	if( iFlags & MEMOBJ_STRING ){` |
|       103 | 1051 | `		return PH7_MemObjToString;` |
|    100177 | 1052 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100113 | 1053 | `		return PH7_MemObjToInteger;` |
|        69 | 1054 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        56 | 1055 | `		return PH7_MemObjToReal;` |
|        15 | 1056 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         7 | 1057 | `		return PH7_MemObjToBool;` |
|         8 | 1058 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 | 1059 | `		return PH7_MemObjToHashmap;` |
|       ! 0 | 1060 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 1061 | `		return PH7_MemObjToObject;` |
|       ! 0 | 1062 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - | 1063 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - | 1064 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - | 1065 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - | 1066 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - | 1067 | `		 * default. */` |
|       ! 0 | 1068 | `		return 0;` |
|         - | 1069 | `	}` |
|         - | 1070 | `	/* NULL cast */` |
|       ! 0 | 1071 | `	return PH7_MemObjToNull;` |
|     50140 | 1072 | `}` |
|         - | 1073 | `/*` |
|         - | 1074 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - | 1075 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - | 1076 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - | 1077 | ` * loose-comparison numeric gate:` |
|         - | 1078 | ` *` |
|         - | 1079 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - | 1080 | ` *` |
|         - | 1081 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - | 1082 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - | 1083 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - | 1084 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - | 1085 | ` * a non-string value.` |
|         - | 1086 | ` */` |
|         - | 1087 | `/*` |
|         - | 1088 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - | 1089 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - | 1090 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - | 1091 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - | 1092 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - | 1093 | ` * and rejects a string with no prefix outright.` |
|         - | 1094 | ` */` |
|   1564016 | 1095 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 | 1096 | `{` |
|         - | 1097 | `	const char *z, *zEnd;` |
|         - | 1098 | `	sxu32 n;` |
|   1564021 | 1099 | `	int bDigit = 0;` |
|   1564021 | 1100 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 1101 | `		return 0;` |
|         - | 1102 | `	}` |
|   1564021 | 1103 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   1564021 | 1104 | `	n = SyBlobLength(&pValue->sBlob);` |
|   1564021 | 1105 | `	if( n == 0 ){` |
|      1250 | 1106 | `		return 0;` |
|         - | 1107 | `	}` |
|   1562775 | 1108 | `	zEnd = z + n;` |
|   1563281 | 1109 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       509 | 1110 | `		z++;` |
|         3 | 1111 | `	}` |
|   1562775 | 1112 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       821 | 1113 | `		z++;` |
|       408 | 1114 | `	}` |
|   4369519 | 1115 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|   2806749 | 1116 | `		z++; bDigit = 1;` |
|         5 | 1117 | `	}` |
|   1562775 | 1118 | `	if( z < zEnd && z[0] == '.' ){` |
|       931 | 1119 | `		z++;` |
|      2461 | 1120 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1535 | 1121 | `			z++; bDigit = 1;` |
|         5 | 1122 | `		}` |
|       463 | 1123 | `	}` |
|         - | 1124 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   1562775 | 1125 | `	if( !bDigit ){` |
|    378779 | 1126 | `		return 0;` |
|         - | 1127 | `	}` |
|         - | 1128 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - | 1129 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|   1184001 | 1130 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       343 | 1131 | `		const char *zExp = z;` |
|       343 | 1132 | `		z++;` |
|       343 | 1133 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|        23 | 1134 | `			z++;` |
|        11 | 1135 | `		}` |
|       343 | 1136 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        20 | 1137 | `			z = zExp;` |
|        11 | 1138 | `		}else{` |
|       765 | 1139 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       443 | 1140 | `				z++;` |
|         3 | 1141 | `			}` |
|         - | 1142 | `		}` |
|       170 | 1143 | `	}` |
|   1184001 | 1144 | `	if( pzTail ){` |
|   1183961 | 1145 | `		*pzTail = z;` |
|    591954 | 1146 | `	}` |
|   1184001 | 1147 | `	return 1;` |
|    781983 | 1148 | `}` |
|         - | 1149 | `/*` |
|         - | 1150 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - | 1151 | ` * (trailing whitespace allowed, nothing else).` |
|         - | 1152 | ` */` |
|    380526 | 1153 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 | 1154 | `{` |
|    380531 | 1155 | `	const char *zTail = 0, *zEnd;` |
|    380531 | 1156 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    377757 | 1157 | `		return 0;` |
|         - | 1158 | `	}` |
|      2779 | 1159 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      2851 | 1160 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        75 | 1161 | `		zTail++;` |
|         3 | 1162 | `	}` |
|      2779 | 1163 | `	return zTail == zEnd ? 1 : 0;` |
|    190254 | 1164 | `}` |
|         - | 1165 | `/*` |
|         - | 1166 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - | 1167 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - | 1168 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - | 1169 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - | 1170 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - | 1171 | ` * those bytes convert to when it did.` |
|         - | 1172 | ` *` |
|         - | 1173 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - | 1174 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - | 1175 | ` * however large it is, because it was always going to be a double, so making it` |
|         - | 1176 | ` * one lost no digits.` |
|         - | 1177 | ` *` |
|         - | 1178 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - | 1179 | ` * intact when this says no.` |
|         - | 1180 | ` */` |
|       852 | 1181 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         3 | 1182 | `{` |
|       855 | 1183 | `	const char *zTail = 0;` |
|       855 | 1184 | `	int iOverflow = 0;` |
|       855 | 1185 | `	*piOverflow = 0;` |
|       855 | 1186 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|       104 | 1187 | `		return FALSE;` |
|         - | 1188 | `	}` |
|       753 | 1189 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 | 1190 | `		return FALSE;` |
|         - | 1191 | `	}` |
|         - | 1192 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - | 1193 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|       753 | 1194 | `	if( MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|        82 | 1195 | `		return FALSE;` |
|         - | 1196 | `	}` |
|       673 | 1197 | `	MemObjStringToInt(pObj,&iOverflow);` |
|       673 | 1198 | `	*piOverflow = iOverflow;` |
|       673 | 1199 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1200 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1201 | `	}` |
|       673 | 1202 | `	return TRUE;` |
|       425 | 1203 | `}` |
|         - | 1204 | `/*` |
|         - | 1205 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1206 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1207 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1208 | ` */` |
|    290232 | 1209 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1210 | `{` |
|    290237 | 1211 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      4097 | 1212 | `		return TRUE;` |
|    286145 | 1213 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      6971 | 1214 | `		return FALSE;` |
|    279179 | 1215 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1216 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    279179 | 1217 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1218 | `	}` |
|         - | 1219 | `	/* NOT REACHED */` |
|       ! 0 | 1220 | `	return FALSE;` |
|    145111 | 1221 | `}` |
|         - | 1222 | `/*` |
|         - | 1223 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1224 | ` * FALSE otherwise.` |
|         - | 1225 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1226 | ` * NULL value.` |
|         - | 1227 | ` * Boolean FALSE.` |
|         - | 1228 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1229 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1230 | ` * An empty array.` |
|         - | 1231 | ` * NOTE` |
|         - | 1232 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1233 | ` */` |
|     51714 | 1234 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1235 | `{` |
|         - | 1236 | ``	/* php's `empty($x)` is `!zend_is_true($x)` -- the same question the bool cast`` |
|         - | 1237 | `	 * asks, and this used to answer it with rules of its own. They disagreed on a` |
|         - | 1238 | ``	 * string of MORE THAN ONE zero: the old walk called every `"0"` run empty, so`` |
|         - | 1239 | ``	 * `empty("00")` was true and `array_filter(["00"])` dropped it, where php`` |
|         - | 1240 | ``	 * keeps both (only `""` and the single byte `"0"` are false there). The`` |
|         - | 1241 | `	 * warning php raises when a NaN is coerced rides the same door, since` |
|         - | 1242 | ``	 * `empty(NAN)` warns there. */`` |
|     51714 | 1243 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     25864 | 1244 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         5 | 1245 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 1246 | `			"unexpected NAN value was coerced to bool");` |
|         2 | 1247 | `	}` |
|     51719 | 1248 | `	return !MemObjIsTruthy(&(*pObj));` |
|         5 | 1249 | `}` |
|         - | 1250 | `/*` |
|         - | 1251 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1252 | ` * or both.` |
|         - | 1253 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1254 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1255 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1256 | ` * and ignore the rest.` |
|         - | 1257 | ` */` |
|   1859683 | 1258 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1259 | `{` |
|   1859688 | 1260 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|   1857692 | 1261 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      1430 | 1262 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|       531 | 1263 | `				pObj->x.iVal = 0;` |
|       264 | 1264 | `			}` |
|      1430 | 1265 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       713 | 1266 | `		}` |
|         - | 1267 | `		/* Already numeric */` |
|   1857692 | 1268 | `		return  SXRET_OK;` |
|         - | 1269 | `	}` |
|      2001 | 1270 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      2001 | 1271 | `		const char *zTail = 0;` |
|      2001 | 1272 | `		int bNum, bReal = 0;` |
|         - | 1273 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1274 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1275 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1276 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      2001 | 1277 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      2001 | 1278 | `		if( bNum ){` |
|      1969 | 1279 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      9283 | 1280 | `			while( z < zTail ){` |
|      7505 | 1281 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       191 | 1282 | `					bReal = 1;` |
|       191 | 1283 | `					break;` |
|         - | 1284 | `				}` |
|      7319 | 1285 | `				z++;` |
|         5 | 1286 | `			}` |
|       978 | 1287 | `		}` |
|      2001 | 1288 | `		if( bReal ){` |
|       191 | 1289 | `			PH7_MemObjToReal(&(*pObj));` |
|        98 | 1290 | `		}else{` |
|      1815 | 1291 | `			if( !bNum ){` |
|         - | 1292 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        33 | 1293 | `				pObj->x.iVal = 0;` |
|        17 | 1294 | `			}else{` |
|      1783 | 1295 | `				int iOverflow = 0;` |
|         - | 1296 | `				/* Convert as much as we can */` |
|      1783 | 1297 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1298 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      1783 | 1299 | `				if( iOverflow ){` |
|         - | 1300 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1301 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1302 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1303 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1304 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1305 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1306 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1307 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1308 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1309 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1310 | `					 * OP_ADD's overflow arm makes there. */` |
|       228 | 1311 | `					PH7_MemObjToReal(&(*pObj));` |
|       228 | 1312 | `					return SXRET_OK;` |
|         - | 1313 | `				}` |
|         - | 1314 | `#endif` |
|         - | 1315 | `			}` |
|      1589 | 1316 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      1589 | 1317 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1318 | `		}` |
|       881 | 1319 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 | 1320 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1321 | `	}else{` |
|         - | 1322 | `		/* Perform a blind cast */` |
|       ! 0 | 1323 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1324 | `	}` |
|      1775 | 1325 | `	return SXRET_OK;` |
|    931089 | 1326 | `}` |
|         - | 1327 | `/*` |
|         - | 1328 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1329 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1330 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1331 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1332 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1333 | ` * last carried character. Empty strings become "1".` |
|         - | 1334 | ` *` |
|         - | 1335 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1336 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1337 | ` * a string even though it looks numeric.` |
|         - | 1338 | ` */` |
|       ! 0 | 1339 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1340 | `{` |
|         - | 1341 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1342 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1343 | `	sxu32 nLen, pos;` |
|         - | 1344 | `	sxu8 *zStr;` |
|       ! 0 | 1345 | `	int carry = 1;` |
|         - | 1346 | `	int ch;` |
|         - | 1347 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1348 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1349 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1350 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1351 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1352 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1353 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1354 | `	}` |
|       ! 0 | 1355 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1356 | `	if( nLen == 0 ){` |
|       ! 0 | 1357 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1358 | `		return SXRET_OK;` |
|         - | 1359 | `	}` |
|       ! 0 | 1360 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1361 | `	pos = nLen;` |
|       ! 0 | 1362 | `	while( pos > 0 ){` |
|       ! 0 | 1363 | `		pos--;` |
|       ! 0 | 1364 | `		ch = zStr[pos];` |
|       ! 0 | 1365 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1366 | `			if( ch == 'z' ){` |
|       ! 0 | 1367 | `				zStr[pos] = 'a';` |
|       ! 0 | 1368 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1369 | `				continue;` |
|         - | 1370 | `			}` |
|       ! 0 | 1371 | `			zStr[pos]++;` |
|       ! 0 | 1372 | `			carry = 0;` |
|       ! 0 | 1373 | `			break;` |
|       ! 0 | 1374 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1375 | `			if( ch == 'Z' ){` |
|       ! 0 | 1376 | `				zStr[pos] = 'A';` |
|       ! 0 | 1377 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1378 | `				continue;` |
|         - | 1379 | `			}` |
|       ! 0 | 1380 | `			zStr[pos]++;` |
|       ! 0 | 1381 | `			carry = 0;` |
|       ! 0 | 1382 | `			break;` |
|       ! 0 | 1383 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1384 | `			if( ch == '9' ){` |
|       ! 0 | 1385 | `				zStr[pos] = '0';` |
|       ! 0 | 1386 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1387 | `				continue;` |
|         - | 1388 | `			}` |
|       ! 0 | 1389 | `			zStr[pos]++;` |
|       ! 0 | 1390 | `			carry = 0;` |
|       ! 0 | 1391 | `			break;` |
|       ! 0 | 1392 | `		}else{` |
|         - | 1393 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1394 | `			carry = 0;` |
|       ! 0 | 1395 | `			break;` |
|         - | 1396 | `		}` |
|       ! 0 | 1397 | `	}` |
|       ! 0 | 1398 | `	if( carry ){` |
|         - | 1399 | `		sxu8 prepend;` |
|         - | 1400 | `		sxu32 i;` |
|       ! 0 | 1401 | `		switch( last_class ){` |
|       ! 0 | 1402 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1403 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1404 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1405 | `		}` |
|         - | 1406 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1407 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1408 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1409 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1410 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1411 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1412 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1413 | `		}` |
|       ! 0 | 1414 | `		zStr[0] = prepend;` |
|       ! 0 | 1415 | `	}` |
|       ! 0 | 1416 | `	return SXRET_OK;` |
|       ! 0 | 1417 | `}` |
|         - | 1418 | `/*` |
|         - | 1419 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1420 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1421 | ` */` |
|      3546 | 1422 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         5 | 1423 | `{` |
|      3551 | 1424 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1425 | `		/* Work only with reals */` |
|      3551 | 1426 | `		MemObjTryIntger(&(*pObj));` |
|      1773 | 1427 | `	}` |
|      3551 | 1428 | `	return SXRET_OK;` |
|         5 | 1429 | `}` |
|         - | 1430 | `/*` |
|         - | 1431 | ` * Initialize a ph7_value to the null type.` |
|         - | 1432 | ` */` |
| 133451401 | 1433 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1434 | `{` |
|         - | 1435 | `	/* Zero the structure */` |
| 133451406 | 1436 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1437 | `	/* Initialize fields */` |
| 133451406 | 1438 | `	pObj->pVm = pVm;` |
| 133451406 | 1439 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1440 | `	/* Set the NULL type */` |
| 133451406 | 1441 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 133451406 | 1442 | `	return SXRET_OK;` |
|         5 | 1443 | `}` |
|         - | 1444 | `/*` |
|         - | 1445 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1446 | ` */` |
|   7975052 | 1447 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1448 | `{` |
|         - | 1449 | `	/* Zero the structure */` |
|   7975057 | 1450 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1451 | `	/* Initialize fields */` |
|   7975057 | 1452 | `	pObj->pVm = pVm;` |
|   7975057 | 1453 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1454 | `	/* Set the desired type */` |
|   7975057 | 1455 | `	pObj->x.iVal = iVal;` |
|   7975057 | 1456 | `	pObj->iFlags = MEMOBJ_INT;` |
|   7975057 | 1457 | `	return SXRET_OK;` |
|         5 | 1458 | `}` |
|         - | 1459 | `/*` |
|         - | 1460 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1461 | ` */` |
|     30154 | 1462 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1463 | `{` |
|         - | 1464 | `	/* Zero the structure */` |
|     30159 | 1465 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1466 | `	/* Initialize fields */` |
|     30159 | 1467 | `	pObj->pVm = pVm;` |
|     30159 | 1468 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1469 | `	/* Set the desired type */` |
|     30159 | 1470 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     30159 | 1471 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     30159 | 1472 | `	return SXRET_OK;` |
|         5 | 1473 | `}` |
|         - | 1474 | `/*` |
|         - | 1475 | ` * Initialize a ph7_value to the real type.` |
|         - | 1476 | ` */` |
|      1438 | 1477 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         4 | 1478 | `{` |
|         - | 1479 | `	/* Zero the structure */` |
|      1442 | 1480 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1481 | `	/* Initialize fields */` |
|      1442 | 1482 | `	pObj->pVm = pVm;` |
|      1442 | 1483 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1484 | `	/* Set the desired type */` |
|      1442 | 1485 | `	pObj->rVal = rVal;` |
|      1442 | 1486 | `	pObj->iFlags = MEMOBJ_REAL;` |
|      1442 | 1487 | `	return SXRET_OK;` |
|         4 | 1488 | `}` |
|         - | 1489 | `/*` |
|         - | 1490 | ` * Initialize a ph7_value to the array type.` |
|         - | 1491 | ` */` |
|   4442960 | 1492 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1493 | `{` |
|         - | 1494 | `	/* Zero the structure */` |
|   4442965 | 1495 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1496 | `	/* Initialize fields */` |
|   4442965 | 1497 | `	pObj->pVm = pVm;` |
|   4442965 | 1498 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1499 | `	/* Set the desired type */` |
|   4442965 | 1500 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   4442965 | 1501 | `	pObj->x.pOther = pArray;` |
|   4442965 | 1502 | `	return SXRET_OK;` |
|         5 | 1503 | `}` |
|         - | 1504 | `/*` |
|         - | 1505 | ` * Initialize a ph7_value to the string type.` |
|         - | 1506 | ` */` |
|  11931873 | 1507 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1508 | `{` |
|         - | 1509 | `	/* Zero the structure */` |
|  11931878 | 1510 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1511 | `	/* Initialize fields */` |
|  11931878 | 1512 | `	pObj->pVm = pVm;` |
|  11931878 | 1513 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  11931878 | 1514 | `	if( pVal ){` |
|         - | 1515 | `		/* Append contents */` |
|   8335676 | 1516 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   4167840 | 1517 | `	}` |
|         - | 1518 | `	/* Set the desired type */` |
|  11931878 | 1519 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  11931878 | 1520 | `	return SXRET_OK;` |
|         5 | 1521 | `}` |
|         - | 1522 | `/*` |
|         - | 1523 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1524 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1525 | ` * invalidate any prior representation and set the string type.` |
|         - | 1526 | ` * Then a simple append operation is performed.` |
|         - | 1527 | ` */` |
|   4304400 | 1528 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1529 | `{` |
|         - | 1530 | `	sxi32 rc;` |
|   4304405 | 1531 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1532 | `		/* Invalidate any prior representation */` |
|     44929 | 1533 | `		PH7_MemObjRelease(pObj);` |
|     44929 | 1534 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     22457 | 1535 | `	}` |
|         - | 1536 | `	/* Append contents */` |
|   4304405 | 1537 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   4304405 | 1538 | `	return rc;` |
|         5 | 1539 | `}` |
|         - | 1540 | `#if 0` |
|         - | 1541 | `/*` |
|         - | 1542 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1543 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1544 | ` * any prior representation and set the string type.` |
|         - | 1545 | ` * Then a simple format and append operation is performed.` |
|         - | 1546 | ` */` |
|         - | 1547 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1548 | `{` |
|         - | 1549 | `	sxi32 rc;` |
|         - | 1550 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1551 | `		/* Invalidate any prior representation */` |
|         - | 1552 | `		PH7_MemObjRelease(pObj);` |
|         - | 1553 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1554 | `	}` |
|         - | 1555 | `	/* Format and append contents */` |
|         - | 1556 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1557 | `	return rc;` |
|         - | 1558 | `}` |
|         - | 1559 | `#endif` |
|         - | 1560 | `/*` |
|         - | 1561 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1562 | ` */` |
|  28096139 | 1563 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1564 | `{` |
|  28096144 | 1565 | `	ph7_class_instance *pObj = 0;` |
|  28096144 | 1566 | `	ph7_hashmap *pMap = 0;` |
|         - | 1567 | `	sxi32 rc;` |
|  28096144 | 1568 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1569 | `		/* Increment reference count */` |
|   3993192 | 1570 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  26099550 | 1571 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1572 | `		/* Increment reference count */` |
|    103512 | 1573 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     51746 | 1574 | `	}` |
|  28096144 | 1575 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    125795 | 1576 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  28033249 | 1577 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     33630 | 1578 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|     16817 | 1579 | `	}` |
|  28096144 | 1580 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  28096144 | 1581 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  28096144 | 1582 | `	rc = SXRET_OK;` |
|  28096144 | 1583 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  12975618 | 1584 | `		SyBlobReset(&pDest->sBlob);` |
|  12975618 | 1585 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   6489821 | 1586 | `	}else{` |
|  15120531 | 1587 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   2825985 | 1588 | `			SyBlobRelease(&pDest->sBlob);` |
|   1413953 | 1589 | `		}` |
|         - | 1590 | `	}` |
|  28096144 | 1591 | `	if( pMap ){` |
|    125795 | 1592 | `		PH7_HashmapUnref(pMap);` |
|  28033249 | 1593 | `	}else if( pObj ){` |
|     33630 | 1594 | `		PH7_ClassInstanceUnref(pObj);` |
|     16817 | 1595 | `	}` |
|  28096139 | 1596 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  16049428 | 1597 | `	 && pDest->pVm` |
|   3993187 | 1598 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1599 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1600 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1601 | `	  * for closure envs and other non-slot destinations. */` |
|   1996602 | 1602 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1603 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1604 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1605 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1606 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1607 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1608 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1609 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1610 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1611 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1612 | `			pDest->x.pOther = pSnap;` |
|         4 | 1613 | `		}else if( pSnap ){` |
|       ! 0 | 1614 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1615 | `		}` |
|         4 | 1616 | `	}` |
|  28096144 | 1617 | `	return rc;` |
|         5 | 1618 | `}` |
|         - | 1619 | `/*` |
|         - | 1620 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1621 | ` * buffer contents,simply point to it.` |
|         - | 1622 | ` */` |
|  30565502 | 1623 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1624 | `{` |
|  30565507 | 1625 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1626 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1627 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1628 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1629 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1630 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1631 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  30565507 | 1632 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  30565507 | 1633 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1634 | `		/* Increment reference count */` |
|    980549 | 1635 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  30075234 | 1636 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1637 | `		/* Increment reference count */` |
|    502926 | 1638 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    251466 | 1639 | `	}` |
|  30565507 | 1640 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       209 | 1641 | `		SyBlobRelease(&pDest->sBlob);` |
|       102 | 1642 | `	}` |
|  30565507 | 1643 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  16973932 | 1644 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   8493135 | 1645 | `	}` |
|  30565507 | 1646 | `	return SXRET_OK;` |
|         5 | 1647 | `}` |
|         - | 1648 | `/*` |
|         - | 1649 | ` * Read a value WITHOUT converting the caller's copy of it.` |
|         - | 1650 | ` *` |
|         - | 1651 | ` * Every ph7_value_to_xxx()/PH7_MemObjToXxx() is destructive: it rewrites the` |
|         - | 1652 | ` * object it is handed and throws the prior representation away. That is right` |
|         - | 1653 | ` * for a VM operand, and wrong for an entry a builtin FETCHED out of an array` |
|         - | 1654 | ` * the script still holds — an $options member, a stream-filter parameter, a` |
|         - | 1655 | ` * proc_open descriptor — where converting in place rewrites the script's own` |
|         - | 1656 | ` * array (php's zval_get_long()/zval_get_string() family never touch theirs).` |
|         - | 1657 | ` *` |
|         - | 1658 | ` * PH7_ValuePeek loads an aliasing copy into pScratch (which the caller must` |
|         - | 1659 | ` * have PH7_MemObjInit'd and must PH7_MemObjRelease afterwards) and answers it,` |
|         - | 1660 | ` * so the destructive conversion lands on the copy. A string read through it` |
|         - | 1661 | ` * stays valid until the scratch value is released. The three scalar wrappers` |
|         - | 1662 | ` * carry their own scratch for the common case.` |
|         - | 1663 | ` */` |
|       262 | 1664 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch)` |
|         3 | 1665 | `{` |
|       265 | 1666 | `	PH7_MemObjLoad(pVal,pScratch);` |
|       265 | 1667 | `	return pScratch;` |
|         3 | 1668 | `}` |
|      1694 | 1669 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal)` |
|         5 | 1670 | `{` |
|         - | 1671 | `	ph7_value sTmp;` |
|         - | 1672 | `	sxi64 iVal;` |
|      1699 | 1673 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|      1699 | 1674 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|      1699 | 1675 | `	PH7_MemObjToInteger(&sTmp);` |
|      1699 | 1676 | `	iVal = sTmp.x.iVal;` |
|      1699 | 1677 | `	PH7_MemObjRelease(&sTmp);` |
|      1699 | 1678 | `	return iVal;` |
|         5 | 1679 | `}` |
|         - | 1680 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       522 | 1681 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal)` |
|         2 | 1682 | `{` |
|         - | 1683 | `	ph7_value sTmp;` |
|         - | 1684 | `	ph7_real rVal;` |
|       524 | 1685 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|       524 | 1686 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|       524 | 1687 | `	PH7_MemObjToReal(&sTmp);` |
|       524 | 1688 | `	rVal = sTmp.rVal;` |
|       524 | 1689 | `	PH7_MemObjRelease(&sTmp);` |
|       524 | 1690 | `	return rVal;` |
|         2 | 1691 | `}` |
|         - | 1692 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         6 | 1693 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal)` |
|         2 | 1694 | `{` |
|         - | 1695 | `	ph7_value sTmp;` |
|         - | 1696 | `	int bVal;` |
|         8 | 1697 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|         8 | 1698 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|         8 | 1699 | `	PH7_MemObjToBool(&sTmp);` |
|         8 | 1700 | `	bVal = sTmp.x.iVal != 0;` |
|         8 | 1701 | `	PH7_MemObjRelease(&sTmp);` |
|         8 | 1702 | `	return bVal;` |
|         2 | 1703 | `}` |
|         - | 1704 | `/*` |
|         - | 1705 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1706 | ` */` |
| 155058522 | 1707 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1708 | `{` |
| 155058527 | 1709 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1710 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1711 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1712 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1713 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1714 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1715 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1716 | `		pObj->x.pOther = 0;` |
|         7 | 1717 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1718 | `	}` |
| 155058527 | 1719 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - | 1720 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|         - | 1721 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|         - | 1722 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|         - | 1723 | `		 * argument list threw never leaks the receiver it was holding. */` |
|       ! 0 | 1724 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|       ! 0 | 1725 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1726 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       ! 0 | 1727 | `	}` |
| 155058527 | 1728 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1729 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1730 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1731 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1732 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|         3 | 1733 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|         3 | 1734 | `		pObj->x.pOther = 0;` |
|         3 | 1735 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|         1 | 1736 | `	}` |
| 155058527 | 1737 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  85904701 | 1738 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   9129550 | 1739 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  81339926 | 1740 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   5248719 | 1741 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   2624351 | 1742 | `		}` |
|         - | 1743 | `		/* Release the internal buffer */` |
|  85904701 | 1744 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1745 | `		/* Invalidate any prior representation */` |
|  85904701 | 1746 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  42970362 | 1747 | `	}` |
| 155058527 | 1748 | `	return SXRET_OK;` |
|         5 | 1749 | `}` |
|         - | 1750 | `/*` |
|         - | 1751 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1752 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1753 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1754 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1755 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1756 | ` *` |
|         - | 1757 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1758 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1759 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1760 | ` *` |
|         - | 1761 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1762 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1763 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1764 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1765 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1766 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1767 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1768 | ` */` |
|       212 | 1769 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         4 | 1770 | `{` |
|       216 | 1771 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       216 | 1772 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       216 | 1773 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|       150 | 1774 | `		if( PH7_MemObjIsNotStringable(pSelf)` |
|       141 | 1775 | `		 \|\| (pSelf->pVm && PH7_CALLBACK_UNWOUND(pSelf->pVm->nBoundaryRc)) ){` |
|         - | 1776 | `			/* php enters no PHP function while an exception is pending` |
|         - | 1777 | `			 * (zend_call_function bails on EG(exception)), so a __toString()` |
|         - | 1778 | `			 * that already threw -- or exited -- is NOT run again: every later` |
|         - | 1779 | `			 * comparison orders this operand the way a refused cast does. A sort` |
|         - | 1780 | `			 * used to re-enter the body once per remaining pair, and where the` |
|         - | 1781 | `			 * enclosing catch had already run in place the second throw was` |
|         - | 1782 | `			 * UNCAUGHT and killed the script. */` |
|        63 | 1783 | `			return FALSE;` |
|         - | 1784 | `		}` |
|        92 | 1785 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        92 | 1786 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1787 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1788 | `			 * point; until then order the operands the way a refused cast does. */` |
|        39 | 1789 | `			return FALSE;` |
|         - | 1790 | `		}` |
|        53 | 1791 | `		return TRUE;` |
|         - | 1792 | `	}` |
|        64 | 1793 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1794 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|        12 | 1795 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|        12 | 1796 | `		return TRUE;` |
|         - | 1797 | `	}` |
|        54 | 1798 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1799 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1800 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1801 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1802 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1803 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|        10 | 1804 | `		}` |
|        21 | 1805 | `		if( bReal ){` |
|         7 | 1806 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 1807 | `		}else{` |
|        15 | 1808 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 1809 | `		}` |
|        21 | 1810 | `		return TRUE;` |
|         - | 1811 | `	}` |
|        34 | 1812 | `	return FALSE;` |
|       110 | 1813 | `}` |
|         - | 1814 | `/*` |
|         - | 1815 | ` * Compare two ph7_values.` |
|         - | 1816 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1817 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1818 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1819 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1820 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1821 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1822 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1823 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1824 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1825 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1826 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1827 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1828 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1829 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1830 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1831 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1832 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1833 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1834 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1835 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1836 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1837 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1838 | ` *      Loose comparisons with ==` |
|         - | 1839 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1840 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1841 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1842 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1843 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1844 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1845 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1846 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1847 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1848 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1849 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1850 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1851 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1852 | ` *    Strict comparisons with ===` |
|         - | 1853 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1854 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1855 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1856 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1857 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1858 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1859 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1860 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1861 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1862 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1863 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1864 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1865 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1866 | ` */` |
|   3512707 | 1867 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1868 | `{` |
|         - | 1869 | `	sxi32 iComb;` |
|         - | 1870 | `	sxi32 rc;` |
|   3512712 | 1871 | `	if( bStrict ){` |
|         - | 1872 | `		sxi32 iF1,iF2;` |
|         - | 1873 | `		/* Strict comparisons with === */` |
|   1931633 | 1874 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   1931633 | 1875 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   1931633 | 1876 | `		if( iF1 != iF2 ){` |
|         - | 1877 | `			/* Not of the same type */` |
|    395043 | 1878 | `			return 1;` |
|         - | 1879 | `		}` |
|    769443 | 1880 | `	}` |
|         - | 1881 | `	/* Combine flag together */` |
|   3117674 | 1882 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   3117669 | 1883 | `	if( !bStrict` |
|   2350522 | 1884 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    791762 | 1885 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        84 | 1886 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1887 | `		/*` |
|         - | 1888 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1889 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1890 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1891 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1892 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1893 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1894 | `		 */` |
|        45 | 1895 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        31 | 1896 | `			PH7_MemObjToString(pObj1);` |
|        16 | 1897 | `		}else{` |
|        15 | 1898 | `			PH7_MemObjToString(pObj2);` |
|         - | 1899 | `		}` |
|        45 | 1900 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1901 | `	}` |
|   3117674 | 1902 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1903 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1904 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        67 | 1905 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        67 | 1906 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        67 | 1907 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1908 | `	}` |
|   3117610 | 1909 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 1910 | `		/*` |
|         - | 1911 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 1912 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 1913 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 1914 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 1915 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 1916 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 1917 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 1918 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 1919 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 1920 | `		 * equalled every open resource.` |
|         - | 1921 | `		 *` |
|         - | 1922 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 1923 | `		 * already answered 1.` |
|         - | 1924 | `		 */` |
|       240 | 1925 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       240 | 1926 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       240 | 1927 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 1928 | `		ph7_value sCast;` |
|         - | 1929 | `		{` |
|         - | 1930 | `			/* ...unless the object's class declares php's compare handler, which is` |
|         - | 1931 | ``			 * asked about a scalar partner too: `Number('1.5') == '1.50'` is TRUE`` |
|         - | 1932 | `			 * where the cast rule would compare two strings. A handler that does not` |
|         - | 1933 | `			 * recognize the value falls through to the cast below. */` |
|       240 | 1934 | `			sxi32 iNative = 1;` |
|       358 | 1935 | `			if( PH7_ClassNativeCmpValue((ph7_class_instance *)pSelf->x.pOther,pOther,` |
|       118 | 1936 | `				!bObj1,&iNative) ){` |
|         - | 1937 | `				/* The hook was told which side it is on and has already flipped its` |
|         - | 1938 | `				 * ordering; the uncomparable 1 is deliberately NOT flipped, which is` |
|         - | 1939 | `				 * what leaves every relational spelling false from both directions. */` |
|        26 | 1940 | `				return iNative;` |
|         - | 1941 | `			}` |
|         - | 1942 | `		}` |
|       216 | 1943 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 1944 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        79 | 1945 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        46 | 1946 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|        84 | 1947 | `			PH7_MemObjRelease(&sCast);` |
|        84 | 1948 | `			return rc;` |
|         - | 1949 | `		}` |
|       134 | 1950 | `		PH7_MemObjRelease(&sCast);` |
|         - | 1951 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|       134 | 1952 | `		return bObj1 ? 1 : -1;` |
|         - | 1953 | `	}` |
|   3117374 | 1954 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|         - | 1955 | `		/* Convert to boolean: Keep in mind FALSE < TRUE. php decides null and bool` |
|         - | 1956 | `		 * this way and nothing else -- a RESOURCE used to be decided here too,` |
|         - | 1957 | `		 * which made every open one equal to every other truthy value. */` |
|     63421 | 1958 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     36797 | 1959 | `			MemObjToBoolQuiet(pObj1);   /* php's comparison says nothing about a NaN */` |
|     18396 | 1960 | `		}` |
|     63421 | 1961 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     35229 | 1962 | `			MemObjToBoolQuiet(pObj2);` |
|     17612 | 1963 | `		}` |
|     63421 | 1964 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   3053958 | 1965 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1966 | `		/* Hashmap aka 'array' comparison */` |
|       426 | 1967 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1968 | `			/* Array is always greater */` |
|        37 | 1969 | `			return -1;` |
|         - | 1970 | `		}` |
|       390 | 1971 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1972 | `			/* Array is always greater */` |
|        21 | 1973 | `			return 1;` |
|         - | 1974 | `		}` |
|         - | 1975 | `		/* Perform the comparison */` |
|       370 | 1976 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       370 | 1977 | `		return rc;` |
|   3053536 | 1978 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1979 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 1980 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 1981 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 1982 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|       899 | 1983 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1984 | `			/* Object is always greater */` |
|       ! 0 | 1985 | `			return -1;` |
|         - | 1986 | `		}` |
|       899 | 1987 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1988 | `			/* Object is always greater */` |
|       ! 0 | 1989 | `			return 1;` |
|         - | 1990 | `		}` |
|         - | 1991 | `		/* Perform the comparison */` |
|       899 | 1992 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       899 | 1993 | `		return rc;` |
|   3052642 | 1994 | `	}else if( !VmIsUnorderedCmp(pObj1,pObj2) && (iComb & MEMOBJ_RES) ){` |
|         - | 1995 | `		/* php compares a resource with a NON-resource as its ID -- the number` |
|         - | 1996 | ``		 * `(int)$fp` answers -- and the other side takes php's LEGACY`` |
|         - | 1997 | `		 * scalar-to-number conversion, not php 8's saner string rule, because` |
|         - | 1998 | ``		 * this comparison never reaches that rule: `$fp == "5abc"` is TRUE for`` |
|         - | 1999 | ``		 * resource #5 and `$fp > "x"` compares 5 with 0. PHL compared the pair as`` |
|         - | 2000 | `		 * BOOLEANS, so an open resource equalled every non-empty string, every` |
|         - | 2001 | `		 * non-zero number and every other open resource, was GREATER than the` |
|         - | 2002 | ``		 * empty array, and `max($fp, 10)` answered the resource.`` |
|         - | 2003 | `		 *` |
|         - | 2004 | `		 * The two-resource case is decided above (by ID), null and bool before` |
|         - | 2005 | `		 * that (php's bool comparison), an array above this (an array is` |
|         - | 2006 | `		 * greater), an object by the cast rule, and a NaN by the unordered one --` |
|         - | 2007 | `		 * exactly php's order. */` |
|       229 | 2008 | `		int bRes1 = (pObj1->iFlags & MEMOBJ_RES) != 0;` |
|       229 | 2009 | `		ph7_value *pRes = bRes1 ? pObj1 : pObj2;` |
|       229 | 2010 | `		ph7_value *pOther = bRes1 ? pObj2 : pObj1;` |
|       229 | 2011 | `		sxi64 iId = (sxi64)PH7_VmResourceId(pRes->pVm,pRes->x.pOther);` |
|       229 | 2012 | `		PH7_MemObjToNumeric(pOther);` |
|       229 | 2013 | `		if( pOther->iFlags & MEMOBJ_REAL ){` |
|        57 | 2014 | `			ph7_real rId = (ph7_real)iId;` |
|        57 | 2015 | `			rc = rId > pOther->rVal ? 1 : (rId < pOther->rVal ? -1 : 0);` |
|        29 | 2016 | `		}else{` |
|       173 | 2017 | `			rc = iId > pOther->x.iVal ? 1 : (iId < pOther->x.iVal ? -1 : 0);` |
|         - | 2018 | `		}` |
|       229 | 2019 | `		return bRes1 ? rc : -rc;` |
|   3052414 | 2020 | `	}else if( VmIsUnorderedCmp(pObj1,pObj2) ){` |
|         - | 2021 | `		/* A NaN against a number or a string: php answers 1 in BOTH directions` |
|         - | 2022 | ``		 * (`NAN <=> 1` and `1 <=> NAN` are both 1), which is what leaves every`` |
|         - | 2023 | `		 * relational operator false at once. The rule lives HERE, not only in the` |
|         - | 2024 | `		 * operator arms, because everything else that orders values goes through` |
|         - | 2025 | ``		 * this comparator with no arm of its own: `in_array(NAN, ["NAN"])` was TRUE`` |
|         - | 2026 | ``		 * (php: false), `array_search` found it, and a `switch` matched it -- all`` |
|         - | 2027 | `		 * because the string branch below rendered the NaN as the bytes "NAN" and` |
|         - | 2028 | `		 * compared those. The precedence php gives null, bool, array and object is` |
|         - | 2029 | `		 * already spent above: VmIsUnorderedCmp screens those flags out, so` |
|         - | 2030 | ``		 * `NAN == true` stays the bool comparison it is there. */`` |
|       240 | 2031 | `		return 1;` |
|   3052176 | 2032 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 2033 | `		SyString s1,s2;` |
|   1321394 | 2034 | `		if( !bStrict ){` |
|         - | 2035 | `			/*` |
|         - | 2036 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 2037 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 2038 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 2039 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 2040 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 2041 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 2042 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 2043 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 2044 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 2045 | `			 * below, unchanged.` |
|         - | 2046 | `			 */` |
|    274639 | 2047 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 2048 | `				/*` |
|         - | 2049 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 2050 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 2051 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 2052 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 2053 | `				 * really does compare as doubles, so` |
|         - | 2054 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 2055 | `				 *` |
|         - | 2056 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 2057 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 2058 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 2059 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 2060 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 2061 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 2062 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 2063 | `				 *    one, no conversion involved -- which is why` |
|         - | 2064 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 2065 | `				 *    reach the same double.` |
|         - | 2066 | `				 *` |
|         - | 2067 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 2068 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 2069 | `				 */` |
|       429 | 2070 | `				int bBytes = 0;` |
|         - | 2071 | `				{` |
|       429 | 2072 | `					ph7_real r1 = 0, r2 = 0;` |
|       429 | 2073 | `					int iOf1 = 0, iOf2 = 0;` |
|       429 | 2074 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|       429 | 2075 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|       429 | 2076 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 2077 | `						bBytes = 1;` |
|       379 | 2078 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        36 | 2079 | `						return iOf1;` |
|       303 | 2080 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        19 | 2081 | `						return -iOf2;` |
|         - | 2082 | `					}` |
|         - | 2083 | `				}` |
|       385 | 2084 | `				if( !bBytes ){` |
|         - | 2085 | `					/* Perform a numeric comparison */` |
|       285 | 2086 | `					goto Numeric;` |
|         - | 2087 | `				}` |
|        50 | 2088 | `			}` |
|    137148 | 2089 | `		}` |
|         - | 2090 | `		/* Perform a strict string comparison.*/` |
|   1321068 | 2091 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        25 | 2092 | `			PH7_MemObjToString(pObj1);` |
|        12 | 2093 | `		}` |
|   1321068 | 2094 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        35 | 2095 | `			PH7_MemObjToString(pObj2);` |
|        17 | 2096 | `		}` |
|   1321068 | 2097 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   1321068 | 2098 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 2099 | `		/*` |
|         - | 2100 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 2101 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 2102 | `		 */` |
|   1321068 | 2103 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   1321068 | 2104 | `		if( rc == 0 ){` |
|    416177 | 2105 | `			if( s1.nByte != s2.nByte ){` |
|     26365 | 2106 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     13180 | 2107 | `			}` |
|    208101 | 2108 | `		}` |
|   1321068 | 2109 | `		return rc;` |
|   1730787 | 2110 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    864212 | 2111 | `Numeric:` |
|         - | 2112 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|   1731069 | 2113 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       225 | 2114 | `			PH7_MemObjToNumeric(pObj1);` |
|       109 | 2115 | `		}` |
|   1731069 | 2116 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       243 | 2117 | `			PH7_MemObjToNumeric(pObj2);` |
|       118 | 2118 | `		}` |
|   1731069 | 2119 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 2120 | `			/*` |
|         - | 2121 | `			 * Symisc eXtension to the PHP language:` |
|         - | 2122 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 2123 | `			 */` |
|         - | 2124 | `			ph7_real r1,r2;` |
|         - | 2125 | `			/* Compare as reals */` |
|       850 | 2126 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        45 | 2127 | `				PH7_MemObjToReal(pObj1);` |
|        22 | 2128 | `			}` |
|       850 | 2129 | `			r1 = pObj1->rVal;` |
|       850 | 2130 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        27 | 2131 | `				PH7_MemObjToReal(pObj2);` |
|        13 | 2132 | `			}` |
|       850 | 2133 | `			r2 = pObj2->rVal;` |
|       850 | 2134 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 2135 | `				/*` |
|         - | 2136 | `				 * php's answer for an unordered pair, from either side: 1. The` |
|         - | 2137 | `				 * branch above catches every NaN that arrives AS a float; this one` |
|         - | 2138 | `				 * is for a NaN that only appears once both operands have been` |
|         - | 2139 | `				 * converted, and it must agree with it -- an antisymmetric answer` |
|         - | 2140 | ``				 * here (the old `NaN equals NaN, and is greater than everything`` |
|         - | 2141 | ``				 * else`) is what made `NAN === NAN` true and `1.5 > NAN` disagree`` |
|         - | 2142 | ``				 * with `NAN < 1.5`.`` |
|         - | 2143 | `				 */` |
|       ! 0 | 2144 | `				return 1;` |
|         - | 2145 | `			}` |
|       850 | 2146 | `			if( r1 > r2 ){` |
|        45 | 2147 | `				return 1;` |
|       808 | 2148 | `			}else if( r1 < r2 ){` |
|       228 | 2149 | `				return -1;` |
|         - | 2150 | `			}` |
|       583 | 2151 | `			return 0;` |
|       ! 0 | 2152 | `		}else{` |
|         - | 2153 | `			/* Integer comparison */` |
|   1730223 | 2154 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|    293393 | 2155 | `				return 1;` |
|   1436835 | 2156 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|   1016347 | 2157 | `				return -1;` |
|         - | 2158 | `			}` |
|    420493 | 2159 | `			return 0;` |
|         - | 2160 | `		}` |
|         - | 2161 | `	}` |
|         - | 2162 | `	/* NOT REACHED */` |
|       ! 0 | 2163 | `	return 0;` |
|   1758663 | 2164 | `}` |
|         - | 2165 | `/*` |
|         - | 2166 | ` * Perform an addition operation of two ph7_values.` |
|         - | 2167 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 2168 | ` * is that the '+' operator is overloaded.` |
|         - | 2169 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 2170 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 2171 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 2172 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 2173 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 2174 | ` * be ignored.` |
|         - | 2175 | ` * This function take care of handling all the scenarios.` |
|         - | 2176 | ` */` |
|    461274 | 2177 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 2178 | `{` |
|    461279 | 2179 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2180 | `			/* Arithemtic operation */` |
|    455993 | 2181 | `			PH7_MemObjToNumeric(pObj1);` |
|    455993 | 2182 | `			PH7_MemObjToNumeric(pObj2);` |
|    455993 | 2183 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 2184 | `				/* Floating point arithmetic */` |
|         - | 2185 | `				ph7_real a,b;` |
|       131 | 2186 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        38 | 2187 | `					PH7_MemObjToReal(pObj1);` |
|        18 | 2188 | `				}` |
|       131 | 2189 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        57 | 2190 | `					PH7_MemObjToReal(pObj2);` |
|        28 | 2191 | `				}` |
|       131 | 2192 | `				a = pObj1->rVal;` |
|       131 | 2193 | `				b = pObj2->rVal;` |
|       131 | 2194 | `				pObj1->rVal = a+b;` |
|       131 | 2195 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2196 | `				/* Try to get an integer representation also */` |
|       131 | 2197 | `				MemObjTryIntger(&(*pObj1));` |
|        67 | 2198 | `			}else{` |
|         - | 2199 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 2200 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 2201 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 2202 | `				sxi64 a,b,r;` |
|    455865 | 2203 | `				a = pObj1->x.iVal;` |
|    455865 | 2204 | `				b = pObj2->x.iVal;` |
|    455865 | 2205 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 2206 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        63 | 2207 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        63 | 2208 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2209 | `#else` |
|         - | 2210 | `					pObj1->x.iVal = r;` |
|         - | 2211 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2212 | `#endif` |
|        32 | 2213 | `				}else{` |
|    455803 | 2214 | `					pObj1->x.iVal = r;` |
|    455803 | 2215 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2216 | `				}` |
|         - | 2217 | `			}` |
|    228176 | 2218 | `	}else{` |
|      5291 | 2219 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 2220 | `			ph7_hashmap *pMap;` |
|         - | 2221 | `			sxi32 rc;` |
|      5291 | 2222 | `			if( bAddStore ){` |
|         - | 2223 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 2224 | `				 */` |
|         5 | 2225 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2226 | `					/* Force a hashmap cast */` |
|       ! 0 | 2227 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 2228 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2229 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2230 | `						return rc;` |
|         - | 2231 | `					}` |
|       ! 0 | 2232 | `				}` |
|         - | 2233 | `				/* COW separate before in-place mutation */` |
|         5 | 2234 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         3 | 2235 | `			}else{` |
|         - | 2236 | `				/* Create a new hashmap */` |
|      5287 | 2237 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      5287 | 2238 | `				if( pMap == 0){` |
|       ! 0 | 2239 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2240 | `					return SXERR_MEM;` |
|         - | 2241 | `				}` |
|         - | 2242 | `			}` |
|      5291 | 2243 | `			if( !bAddStore ){` |
|      5287 | 2244 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 2245 | `					/* Perform a hashmap duplication */` |
|      5287 | 2246 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2646 | 2247 | `				}else{` |
|       ! 0 | 2248 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2249 | `						/* Simple insertion */` |
|       ! 0 | 2250 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 2251 | `					}` |
|         - | 2252 | `				}` |
|      2641 | 2253 | `			}` |
|         - | 2254 | `			/* Perform the union */` |
|      5291 | 2255 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      5291 | 2256 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2648 | 2257 | `			}else{` |
|       ! 0 | 2258 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2259 | `					/* Simple insertion */` |
|       ! 0 | 2260 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 2261 | `				}` |
|         - | 2262 | `			}` |
|         - | 2263 | `			/* Reflect the change */` |
|      5291 | 2264 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 2265 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 2266 | `			}` |
|      5291 | 2267 | `			pObj1->x.pOther = pMap;` |
|      5291 | 2268 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2643 | 2269 | `		}` |
|         - | 2270 | `	}` |
|    461279 | 2271 | `	return SXRET_OK;` |
|    230819 | 2272 | `}` |
|         - | 2273 | `/*` |
|         - | 2274 | ` * Return a printable representation of the type of a given` |
|         - | 2275 | ` * ph7_value.` |
|         - | 2276 | ` */` |
|        10 | 2277 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|         2 | 2278 | `{` |
|        12 | 2279 | `	const char *zType = "";` |
|        12 | 2280 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 2281 | `		zType = "null";` |
|        12 | 2282 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 2283 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 2284 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 2285 | `		zType = "double";` |
|        12 | 2286 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 2287 | `		zType = "int";` |
|        12 | 2288 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|         8 | 2289 | `		zType = "string";` |
|         9 | 2290 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2291 | `		zType = "bool";` |
|         6 | 2292 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 2293 | `		zType = "array";` |
|         2 | 2294 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 2295 | `		zType = "object";` |
|       ! 0 | 2296 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 2297 | `		zType = "resource";` |
|       ! 0 | 2298 | `	}` |
|        12 | 2299 | `	return zType;` |
|         2 | 2300 | `}` |
|         - | 2301 | `/*` |
|         - | 2302 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 2303 | ` * Store the dump in the given blob.` |
|         - | 2304 | ` */` |
|         - | 2305 | `/*` |
|         - | 2306 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2307 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2308 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2309 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2310 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2311 | ` */` |
|       212 | 2312 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         5 | 2313 | `{` |
|         - | 2314 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2315 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2316 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2317 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2318 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2319 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2320 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2321 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2322 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       217 | 2323 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2324 | `#else` |
|         - | 2325 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2326 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2327 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2328 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2329 | `	}else{` |
|         - | 2330 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2331 | `	}` |
|         - | 2332 | `#endif` |
|       217 | 2333 | `}` |
|         - | 2334 | `/*` |
|         - | 2335 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2336 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2337 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2338 | ` */` |
|       826 | 2339 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         5 | 2340 | `{` |
|         - | 2341 | `	/* print_r RENDERS through the string coercion -- unlike var_dump and` |
|         - | 2342 | `	 * var_export, which describe the value instead -- so php's NaN warning` |
|         - | 2343 | `	 * belongs here too, once per value it prints. */` |
|       826 | 2344 | `	if( (pObj->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       421 | 2345 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         3 | 2346 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 2347 | `			"unexpected NAN value was coerced to string");` |
|         1 | 2348 | `	}` |
|       831 | 2349 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|        21 | 2350 | `		return;` |
|         - | 2351 | `	}` |
|       811 | 2352 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        13 | 2353 | `		if( pObj->x.iVal != 0 ){` |
|         7 | 2354 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|         3 | 2355 | `		}` |
|        13 | 2356 | `		return;` |
|         - | 2357 | `	}` |
|       799 | 2358 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2359 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2360 | `		 * non-strings into the output) */` |
|       483 | 2361 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       479 | 2362 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       237 | 2363 | `		}` |
|       483 | 2364 | `		return;` |
|         - | 2365 | `	}` |
|       321 | 2366 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       418 | 2367 | `}` |
|     14004 | 2368 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2369 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2370 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2371 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2372 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2373 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2374 | `	int nDepth,        /* Nesting level */` |
|         - | 2375 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2376 | `	)` |
|         5 | 2377 | `{` |
|     14009 | 2378 | `	sxi32 rc = SXRET_OK;` |
|         - | 2379 | `	int i;` |
|     14009 | 2380 | `	if( !ShowType ){` |
|         - | 2381 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2382 | `		 * containers render the Array/Object block (which the container` |
|         - | 2383 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       375 | 2384 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       271 | 2385 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2386 | `		}` |
|       109 | 2387 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       105 | 2388 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2389 | `		}` |
|         5 | 2390 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         5 | 2391 | `		return SXRET_OK;` |
|         - | 2392 | `	}` |
|         - | 2393 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2394 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2395 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     23711 | 2396 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     10077 | 2397 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      5041 | 2398 | `	}` |
|     13639 | 2399 | `	if( isRef ){` |
|        69 | 2400 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        33 | 2401 | `	}` |
|     13639 | 2402 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       247 | 2403 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       247 | 2404 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2405 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|         7 | 2406 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|         7 | 2407 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|         7 | 2408 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|         7 | 2409 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|         3 | 2410 | `			}` |
|         7 | 2411 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         7 | 2412 | `			return SXRET_OK;` |
|         - | 2413 | `		}` |
|       241 | 2414 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       241 | 2415 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       241 | 2416 | `		return rc;` |
|         - | 2417 | `	}` |
|     13397 | 2418 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       577 | 2419 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       577 | 2420 | `		return SXRET_OK;` |
|         - | 2421 | `	}` |
|     12825 | 2422 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      1235 | 2423 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|      1235 | 2424 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1235 | 2425 | `		return rc;` |
|         - | 2426 | `	}` |
|     11595 | 2427 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      3719 | 2428 | `		if( pObj->x.iVal != 0 ){` |
|      2167 | 2429 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|      1086 | 2430 | `		}else{` |
|      1557 | 2431 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2432 | `		}` |
|      3719 | 2433 | `		return SXRET_OK;` |
|         - | 2434 | `	}` |
|      7881 | 2435 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2436 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2437 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       217 | 2438 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       217 | 2439 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       217 | 2440 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       217 | 2441 | `		return SXRET_OK;` |
|         - | 2442 | `	}` |
|      7669 | 2443 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      3399 | 2444 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      3399 | 2445 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      3399 | 2446 | `		return SXRET_OK;` |
|         - | 2447 | `	}` |
|      4275 | 2448 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      4275 | 2449 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      4275 | 2450 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      3991 | 2451 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      1993 | 2452 | `		}` |
|      4275 | 2453 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      4275 | 2454 | `		return SXRET_OK;` |
|         - | 2455 | `	}` |
|       ! 0 | 2456 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2457 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2458 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2459 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2460 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2461 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2462 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2463 | `		return SXRET_OK;` |
|         - | 2464 | `	}` |
|         - | 2465 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2466 | `	{` |
|       ! 0 | 2467 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2468 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2469 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2470 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2471 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2472 | `	}` |
|       ! 0 | 2473 | `	return rc;` |
|      7007 | 2474 | `}` |
|         - | 2475 |  |
