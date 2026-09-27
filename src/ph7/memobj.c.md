# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1086/1232 lines (88.15%)

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
|      1394 |   65 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |   66 | `{` |
|      1399 |   67 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      1273 |   68 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|         - |   69 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|         - |   70 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|         - |   71 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|         - |   72 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|         - |   73 | `	 * A value that IS a float is a float whatever it has cached. */` |
|      1259 |   74 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      1181 |   75 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|       791 |   76 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|       247 |   77 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        35 |   78 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        35 |   79 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |   80 | `	return "unknown";` |
|       702 |   81 | `}` |
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
|     20338 |   99 | `PH7_PRIVATE int PH7_RealFitsInt64(double r)` |
|         5 |  100 | `{` |
|     20343 |  101 | `	return r >= -9223372036854775808.0 && r < 9223372036854775808.0;` |
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
|     19272 |  122 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r)` |
|         5 |  123 | `{` |
|         - |  124 | `  union { double d; sxu64 u; } bits;` |
|         - |  125 | `  sxu64 uMag;` |
|         - |  126 | `  int iShift;` |
|     19277 |  127 | `  if( PH7_RealFitsInt64(r) ){` |
|         - |  128 | `    /* In range: php truncates toward zero, and so does C. */` |
|     17887 |  129 | `    return (sxi64)r;` |
|         - |  130 | `  }` |
|      1394 |  131 | `  if( PH7_IS_NAN(r) \|\| PH7_IS_INF(r) ){` |
|       510 |  132 | `    return 0;` |
|         - |  133 | `  }` |
|       888 |  134 | `  bits.d = r;` |
|         - |  135 | `  /* Unbiased exponent, minus the 52 fraction bits: the power of two the` |
|         - |  136 | `  ** mantissa is scaled by. \|r\| >= 2^63 puts it at 11 or more. */` |
|       888 |  137 | `  iShift = (int)((bits.u >> 52) & 0x7FF) - 1023 - 52;` |
|       888 |  138 | `  if( iShift >= 64 ){` |
|         - |  139 | `    /* Every set bit sits above the 64th, so the residue is 0 -- and the shift` |
|         - |  140 | `    ** below would be undefined. */` |
|       151 |  141 | `    return 0;` |
|         - |  142 | `  }` |
|       740 |  143 | `  uMag = ((bits.u & 0x000FFFFFFFFFFFFFULL) \| 0x0010000000000000ULL) << iShift;` |
|       740 |  144 | `  if( bits.u >> 63 ){` |
|         - |  145 | ``     /* Unsigned negation is the two's-complement residue php's `dmod += 2^64` `` |
|         - |  146 | `    ** arrives at, and is defined for every input including 0. */` |
|        51 |  147 | `    uMag = (sxu64)0 - uMag;` |
|        25 |  148 | `  }` |
|       740 |  149 | `  return (sxi64)uMag;` |
|      9641 |  150 | `}` |
|     18782 |  151 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |  152 | `{` |
|         - |  153 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  154 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |  155 | `	 * is omitted from the build.` |
|         - |  156 | `	 */` |
|         - |  157 | `	return pObj->rVal;` |
|         - |  158 | `#else` |
|     18787 |  159 | `	return PH7_RealToInt64(pObj->rVal);` |
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
|       808 |  182 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r)` |
|         2 |  183 | `{` |
|         - |  184 | `	SyBlob sVal;` |
|         - |  185 | `	char zVal[64];` |
|       810 |  186 | `	if( pVm == 0 \|\| PH7_RealFitsInt64(r) ){` |
|       594 |  187 | `		return;` |
|         - |  188 | `	}` |
|       218 |  189 | `	SyBlobInitFromBuf(&sVal,zVal,(sxu32)sizeof(zVal) - 1);` |
|       218 |  190 | `	PH7_AppendShortestReal(&sVal,r);` |
|       218 |  191 | `	zVal[SyBlobLength(&sVal)] = 0;   /* the blob is LOCKED: it truncates, never grows */` |
|       326 |  192 | `	VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       108 |  193 | `		"The float %s is not representable as an int, cast occurred",zVal);` |
|       406 |  194 | `}` |
|         - |  195 | `/* The same warning asked of a VALUE: only a float can carry one, and the flag` |
|         - |  196 | ` * test mirrors the conversion's own routing (MemObjIntValue reads MEMOBJ_REAL` |
|         - |  197 | ` * first), so the diagnostic and the answer always describe the same branch. */` |
|      3752 |  198 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj)` |
|         5 |  199 | `{` |
|      3757 |  200 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|      3439 |  201 | `		return;` |
|         - |  202 | `	}` |
|       320 |  203 | `	PH7_RealWarnIntCast(pObj->pVm,(double)pObj->rVal);` |
|      1878 |  204 | `}` |
|         - |  205 | `/*` |
|         - |  206 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  207 | ` * to a 64-bit integer.` |
|         - |  208 | ` */` |
|    697882 |  209 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  210 | `{` |
|    697887 |  211 | `	sxi64 iVal = 0;` |
|    697887 |  212 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  213 | `		return 0;` |
|         - |  214 | `	}` |
|    697887 |  215 | `	if( pVal->zString[0] == '0' ){` |
|         - |  216 | `		sxi32 c;` |
|    246695 |  217 | `		if( pVal->nByte == sizeof(char) ){` |
|    240859 |  218 | `			return 0;` |
|         - |  219 | `		}` |
|      5841 |  220 | `		c = pVal->zString[1];` |
|      5841 |  221 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  222 | `			/* Hex digit stream */` |
|       174 |  223 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      5756 |  224 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  225 | `			/* Binary digit stream */` |
|       285 |  226 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      5529 |  227 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  228 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  229 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  230 | `			if( pVal->nByte > 2 ){` |
|        21 |  231 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  232 | `			}` |
|        11 |  233 | `		}else{` |
|         - |  234 | `			/* Legacy octal digit stream (leading 0) */` |
|      5367 |  235 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  236 | `		}` |
|      2923 |  237 | `	}else{` |
|         - |  238 | `		/* Decimal digit stream */` |
|    451197 |  239 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  240 | `	}` |
|    457033 |  241 | `	return iVal;` |
|    348946 |  242 | `}` |
|         - |  243 | `/*` |
|         - |  244 | ` * TRUE when the numeric PREFIX that ends at zTail is float-SHAPED -- it carries` |
|         - |  245 | ` * a '.' or a complete exponent. This is php's is_numeric_string answering` |
|         - |  246 | ` * IS_DOUBLE, and it decides which of two entirely different readings the bytes` |
|         - |  247 | ` * get: an integer-shaped run is read from its DIGITS, a float-shaped one from` |
|         - |  248 | ` * the double they spell.` |
|         - |  249 | ` */` |
|      3854 |  250 | `static int MemObjNumericPrefixIsFloat(ph7_value *pObj,const char *zTail)` |
|         5 |  251 | `{` |
|      3859 |  252 | `	const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     53969 |  253 | `	while( z < zTail ){` |
|     50401 |  254 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       289 |  255 | `			return TRUE;` |
|         - |  256 | `		}` |
|     50115 |  257 | `		z++;` |
|         5 |  258 | `	}` |
|      3573 |  259 | `	return FALSE;` |
|      1920 |  260 | `}` |
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
|       206 |  272 | `static sxi64 MemObjRealToIntCap(ph7_real r)` |
|         2 |  273 | `{` |
|         - |  274 | `	/* NaN fails both comparisons and either infinity fails one of them, so this` |
|         - |  275 | `	 * screens all three without a libm predicate. */` |
|       208 |  276 | `	if( !(r >= -1.7976931348623157e308 && r <= 1.7976931348623157e308) ){` |
|        16 |  277 | `		return 0;` |
|         - |  278 | `	}` |
|       194 |  279 | `	if( r >= 9223372036854775808.0 ){    /* +2^63, exact in double space */` |
|        24 |  280 | `		return LARGEST_INT64;` |
|         - |  281 | `	}` |
|       172 |  282 | `	if( r < -9223372036854775808.0 ){` |
|         5 |  283 | `		return SMALLEST_INT64;` |
|         - |  284 | `	}` |
|       168 |  285 | `	return (sxi64)r;` |
|       105 |  286 | `}` |
|         - |  287 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  288 | `/*` |
|         - |  289 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  290 | ` * do at representing the value that pObj describes as a string` |
|         - |  291 | ` * representation.` |
|         - |  292 | ` */` |
|      3120 |  293 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  294 | `{` |
|      3125 |  295 | `	sxi64 iVal = 0;` |
|         - |  296 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      3125 |  297 | `	const char *zTail = 0;` |
|      3120 |  298 | `	if( PH7_MemObjStringNumericPrefix(pObj,&zTail)` |
|      3117 |  299 | `	 && MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|         - |  300 | `		/* A float-shaped string is a DOUBLE first and an int second, which is the` |
|         - |  301 | ``		 * only reading that makes `(int)"1e3"` the 1000 it says: reading its`` |
|         - |  302 | `		 * digits stops at the 'e' and answers the mantissa's integer part, so` |
|         - |  303 | `		 * "1e3" was 1, "1.5e2" was 1 and "-2e2" was -2. The '.' forms were wrong` |
|         - |  304 | `		 * the same way wherever the double rounds away from the digits --` |
|         - |  305 | ``		 * `(int)"0.9999999999999999999"` is 1, not 0. */`` |
|       208 |  306 | `		ph7_real rVal = 0.0;` |
|       208 |  307 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),` |
|         - |  308 | `			(void *)&rVal,0);` |
|       208 |  309 | `		if( pOverflow ){` |
|         - |  310 | `			/* php reports no overflow for a float-shaped string however large it` |
|         - |  311 | `			 * is: it was always going to be a double, so no digits were lost. */` |
|       ! 0 |  312 | `			*pOverflow = 0;` |
|       ! 0 |  313 | `		}` |
|       208 |  314 | `		return MemObjRealToIntCap(rVal);` |
|         - |  315 | `	}` |
|         - |  316 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  317 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  318 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  319 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|      2919 |  320 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|      2919 |  321 | `	return iVal;` |
|      1557 |  322 | `}` |
|         - |  323 | `/*` |
|         - |  324 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  325 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  326 | ` * successfully called. Any other return value indicates failure.` |
|         - |  327 | ` */` |
|      1396 |  328 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  329 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  330 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  331 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  332 | `	sxu32 nLen,                /* Method name length */` |
|         - |  333 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  334 | `	)` |
|         5 |  335 | `{` |
|         - |  336 | `	ph7_class_method *pMethod;` |
|         - |  337 | `	/* Check if the method is available */` |
|      1401 |  338 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      1401 |  339 | `	if( pMethod == 0 ){` |
|         - |  340 | `		/* No such method */` |
|         5 |  341 | `		return SXERR_NOTFOUND;` |
|         - |  342 | `	}` |
|         - |  343 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  344 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  345 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  346 | `	 * printed "Object" after a caught __toString() throw). */` |
|      1397 |  347 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|       703 |  348 | `}` |
|         - |  349 | `/*` |
|         - |  350 | ` * Return some kind of integer value which is the best we can` |
|         - |  351 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  352 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  353 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  354 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  355 | ` * a integer and return that.` |
|         - |  356 | ` * If pObj represents a NULL value, return 0.` |
|         - |  357 | ` */` |
|      2832 |  358 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  359 | `{` |
|         - |  360 | `	sxi32 iFlags;` |
|      2837 |  361 | `	iFlags = pObj->iFlags;` |
|      2837 |  362 | `	if (iFlags & MEMOBJ_REAL ){` |
|       341 |  363 | `		return MemObjRealToInt(&(*pObj));` |
|      2501 |  364 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       787 |  365 | `		return pObj->x.iVal;` |
|      1717 |  366 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  367 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  368 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  369 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|      1635 |  370 | `		return MemObjStringToInt(&(*pObj),0);` |
|        85 |  371 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        45 |  372 | `		return 0;` |
|        42 |  373 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  374 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  375 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  376 | `		 * followed php; int/float did not.) */` |
|        23 |  377 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        23 |  378 | `		sxu32 n = pMap->nEntry;` |
|        23 |  379 | `		PH7_HashmapUnref(pMap);` |
|        23 |  380 | `		return n > 0 ? 1 : 0;` |
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
|      1421 |  399 | `}` |
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
|     15298 |  410 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  411 | `{` |
|         - |  412 | `	sxi32 iFlags;` |
|     15303 |  413 | `	iFlags = pObj->iFlags;` |
|     15303 |  414 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  415 | `		return pObj->rVal;` |
|     15303 |  416 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      2219 |  417 | `		return (ph7_real)pObj->x.iVal;` |
|     13089 |  418 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  419 | `		SyString sString;` |
|         - |  420 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  421 | `		ph7_real rVal = 0;` |
|         - |  422 | `#else` |
|     13069 |  423 | `		ph7_real rVal = 0.0;` |
|         - |  424 | `#endif` |
|     13069 |  425 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     13069 |  426 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  427 | `			/* Convert as much as we can */` |
|         - |  428 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  429 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  430 | `#else` |
|     13065 |  431 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  432 | `#endif` |
|      6530 |  433 | `		}` |
|     13069 |  434 | `		return rVal;` |
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
|      7654 |  462 | `}` |
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
|       666 |  476 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  477 | `{` |
|         - |  478 | `	sxi32 iExp,i;` |
|       671 |  479 | `	iExp = nLen - 1;` |
|      4929 |  480 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      4263 |  481 | `		iExp--;` |
|         5 |  482 | `	}` |
|       671 |  483 | `	if( iExp <= 0 ){` |
|       615 |  484 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  485 | `	}` |
|         - |  486 | `	{` |
|        58 |  487 | `		sxi32 iDig = iExp + 1;` |
|         - |  488 | `		sxi32 iFirst;` |
|        58 |  489 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|        58 |  490 | `			iDig++;` |
|        28 |  491 | `		}` |
|        58 |  492 | `		iFirst = iDig;` |
|        96 |  493 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|        70 |  494 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        27 |  495 | `			iFirst++;` |
|         1 |  496 | `		}` |
|        58 |  497 | `		if( iFirst > iDig ){` |
|        27 |  498 | `			sxi32 nStrip = iFirst - iDig;` |
|        79 |  499 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        53 |  500 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        27 |  501 | `			}` |
|        27 |  502 | `			nLen -= nStrip;` |
|        13 |  503 | `		}` |
|         - |  504 | `	}` |
|        58 |  505 | `	if( bGeneric ){` |
|        42 |  506 | `		int bHasDot = 0;` |
|        84 |  507 | `		for( i = 0 ; i < iExp ; i++ ){` |
|        56 |  508 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        23 |  509 | `		}` |
|        42 |  510 | `		if( !bHasDot ){` |
|       168 |  511 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       140 |  512 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|        71 |  513 | `			}` |
|        30 |  514 | `			zBuf[iExp] = '.';` |
|        30 |  515 | `			zBuf[iExp+1] = '0';` |
|        30 |  516 | `			nLen += 2;` |
|        14 |  517 | `		}` |
|        20 |  518 | `	}` |
|        58 |  519 | `	return nLen;` |
|       338 |  520 | `}` |
|         - |  521 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  522 | `/*` |
|         - |  523 | ` * Return the string representation of a given ph7_value.` |
|         - |  524 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  525 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  526 | ` * left without a rendering of pObj.` |
|         - |  527 | ` */` |
|     75632 |  528 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  529 | `{` |
|     75637 |  530 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  531 | `		/* Handle special floating-point values first */` |
|       551 |  532 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|        25 |  533 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       539 |  534 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
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
|       517 |  549 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       517 |  550 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  551 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  552 | `			}` |
|       517 |  553 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       517 |  554 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  555 | `#else` |
|         - |  556 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  557 | `#endif` |
|         5 |  558 | `		}` |
|     75364 |  559 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     73193 |  560 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  561 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     38495 |  562 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       199 |  563 | `		if( bStrictBool ){` |
|         - |  564 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|       199 |  565 | `			if( pObj->x.iVal ){` |
|        99 |  566 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|        47 |  567 | `			}` |
|         - |  568 | `			/* false produces empty string, nothing to append */` |
|       102 |  569 | `		}else{` |
|         - |  570 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  571 | `			if( pObj->x.iVal ){` |
|       ! 0 |  572 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  573 | `			}else{` |
|       ! 0 |  574 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  575 | `			}` |
|         5 |  576 | `		}` |
|      1806 |  577 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       123 |  578 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       123 |  579 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      1650 |  580 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  581 | `		ph7_value sResult;` |
|         - |  582 | `		sxi32 rc;` |
|         - |  583 | `		/* Invoke the __toString() method if available */` |
|      1401 |  584 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      1401 |  585 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  586 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      1401 |  587 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  588 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  589 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  590 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  591 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  592 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  593 | `			 * instance reference. */` |
|       193 |  594 | `			PH7_MemObjRelease(&sResult);` |
|       193 |  595 | `			return rc;` |
|         - |  596 | `		}` |
|      1213 |  597 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  598 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  599 | `			 * value, and requiring a non-empty one sent` |
|         - |  600 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  601 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  602 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  603 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  604 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  605 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  606 | `			 * result no return-type check governs. */` |
|      1209 |  607 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|       607 |  608 | `		}else{` |
|         - |  609 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  610 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         5 |  611 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  612 | `		}` |
|      1213 |  613 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      1213 |  614 | `		PH7_MemObjRelease(&sResult);` |
|       799 |  615 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  616 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  617 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|         5 |  618 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         2 |  619 | `	}` |
|     75449 |  620 | `	return SXRET_OK;` |
|     37819 |  621 | `}` |
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
|    120264 |  637 | `static sxi32 MemObjIsTruthy(ph7_value *pObj)` |
|         5 |  638 | `{` |
|         - |  639 | `	sxi32 iFlags;` |
|    120269 |  640 | `	iFlags = pObj->iFlags;` |
|    120269 |  641 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  642 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  643 | `		return pObj->rVal ? 1 : 0;` |
|         - |  644 | `#else` |
|         - |  645 | `		/* A NaN is neither zero nor equal to itself, so it is TRUE -- php's` |
|         - |  646 | `		 * answer too, behind the warning PH7_MemObjToBool raises. */` |
|        99 |  647 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  648 | `#endif` |
|    120173 |  649 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         - |  650 | ``		/* BOOL is here for `empty()`, which asks this of a value of ANY type; the`` |
|         - |  651 | `		 * bool CONVERSION never does (it returns early when the bit is set). */` |
|      1207 |  652 | `		return pObj->x.iVal ? 1 : 0;` |
|    118971 |  653 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  654 | `		SyString sString;` |
|     32969 |  655 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  656 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|     32969 |  657 | `		if( sString.nByte == 0 ){` |
|     25859 |  658 | `			return 0;` |
|         - |  659 | `		}` |
|      7115 |  660 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        20 |  661 | `			return 0;` |
|         - |  662 | `		}` |
|      7097 |  663 | `		return 1;` |
|     86007 |  664 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     67729 |  665 | `		return 0;` |
|     18283 |  666 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     16715 |  667 | `		return ((ph7_hashmap *)pObj->x.pOther)->nEntry > 0 ? TRUE : FALSE;` |
|      1573 |  668 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  669 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  670 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  671 | `		 * extension changed control flow in valid php source. */` |
|       196 |  672 | `		return 1;` |
|      1381 |  673 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1381 |  674 | `		return pObj->x.pOther != 0;` |
|         - |  675 | `	}` |
|         - |  676 | `	/* NOT REACHED */` |
|       ! 0 |  677 | `	return 0;` |
|     60134 |  678 | `}` |
|         - |  679 | `/*` |
|         - |  680 | ` * The same question asked by a CONVERSION, which is about to overwrite the` |
|         - |  681 | ` * payload and so owes it a reference drop. Nothing else about the answer` |
|         - |  682 | `` * differs -- which is the point: `empty()` and `array_filter()`'s default test`` |
|         - |  683 | ` * used to carry a SECOND set of rules (PH7_MemObjIsEmpty), and it disagreed.` |
|         - |  684 | ` */` |
|     70754 |  685 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  686 | `{` |
|     70759 |  687 | `	sxi32 rc = MemObjIsTruthy(&(*pObj));` |
|     70759 |  688 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        44 |  689 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|     70738 |  690 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       188 |  691 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|        92 |  692 | `	}` |
|     70759 |  693 | `	return rc;` |
|         5 |  694 | `}` |
|         - |  695 | `/*` |
|         - |  696 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  697 | ` */` |
|     18446 |  698 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  699 | `{` |
|     18451 |  700 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  701 | `  /* Only mark the value as an integer if` |
|         - |  702 | `  **` |
|         - |  703 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  704 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  705 | `  **        possible integer` |
|         - |  706 | `  **` |
|         - |  707 | `  ** The second and third terms in the following conditional enforces` |
|         - |  708 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  709 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  710 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  711 | `  ** architectures might behave differently.` |
|         - |  712 | `  */` |
|     18446 |  713 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     14208 |  714 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     14183 |  715 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|      7091 |  716 | `	}` |
|     18451 |  717 | `	return SXRET_OK;` |
|         5 |  718 | `}` |
|         - |  719 | `/*` |
|         - |  720 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  721 | ` */` |
|    907333 |  722 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  723 | `{` |
|    907338 |  724 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  725 | `		/* Preform the conversion */` |
|      2837 |  726 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  727 | `		/* Invalidate any prior representations */` |
|      2837 |  728 | `		SyBlobRelease(&pObj->sBlob);` |
|      2837 |  729 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|      1416 |  730 | `	}` |
|    907338 |  731 | `	return SXRET_OK;` |
|         5 |  732 | `}` |
|         - |  733 | `/*` |
|         - |  734 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  735 | ` * Invalidate any prior representations` |
|         - |  736 | ` */` |
|     17336 |  737 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  738 | `{` |
|     17341 |  739 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  740 | `		/* Preform the conversion */` |
|     15303 |  741 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  742 | `		/* Invalidate any prior representations */` |
|     15303 |  743 | `		SyBlobRelease(&pObj->sBlob);` |
|     15303 |  744 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  745 | `		/* Try to get an integer representation */` |
|     15303 |  746 | `		MemObjTryIntger(&(*pObj));` |
|      7649 |  747 | `	}` |
|     17341 |  748 | `	return SXRET_OK;` |
|         5 |  749 | `}` |
|         - |  750 | `/*` |
|         - |  751 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  752 | ` */` |
|     96585 |  753 | `static sxi32 MemObjToBoolQuiet(ph7_value *pObj)` |
|         5 |  754 | `{` |
|     96590 |  755 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  756 | `		/* Preform the conversion */` |
|     70759 |  757 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  758 | `		/* Invalidate any prior representations */` |
|     70759 |  759 | `		SyBlobRelease(&pObj->sBlob);` |
|     70759 |  760 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     35374 |  761 | `	}` |
|     96590 |  762 | `	return SXRET_OK;` |
|         5 |  763 | `}` |
|         - |  764 | `/*` |
|         - |  765 | ` * The same conversion where a php PROGRAM asked for it, which is every` |
|         - |  766 | `` * truthiness site there is: `(bool)`, `if`, `!`, `&&`, the ternary, `empty()`,`` |
|         - |  767 | `` * `boolval()`, `settype()`, a `bool` parameter internal or userland,`` |
|         - |  768 | `` * `array_filter`'s default test. php 8.5 warns from all of them when the value`` |
|         - |  769 | `` * is a NaN -- `unexpected NAN value was coerced to bool` -- and answers TRUE.`` |
|         - |  770 | ` *` |
|         - |  771 | `` * A COMPARISON is not one of them: `NAN == true` is silent in php, and it`` |
|         - |  772 | ` * reaches the same conversion, which is why the quiet form above exists.` |
|         - |  773 | ` */` |
|     27651 |  774 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  775 | `{` |
|     27651 |  776 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     13860 |  777 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        21 |  778 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - |  779 | `			"unexpected NAN value was coerced to bool");` |
|        10 |  780 | `	}` |
|     27656 |  781 | `	return MemObjToBoolQuiet(&(*pObj));` |
|         5 |  782 | `}` |
|         - |  783 | `/*` |
|         - |  784 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  785 | ` */` |
|   4658656 |  786 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  787 | `{` |
|   4658661 |  788 | `	sxi32 rc = SXRET_OK;` |
|   4658661 |  789 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  790 | `		/* Perform the conversion */` |
|     75419 |  791 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     75419 |  792 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     75419 |  793 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  794 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  795 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  796 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  797 | `			 * leaves $o holding the object after the throw is caught. */` |
|       193 |  798 | `			return rc;` |
|         - |  799 | `		}` |
|     75231 |  800 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     37611 |  801 | `	}` |
|   4658473 |  802 | `	return rc;` |
|   2331194 |  803 | `}` |
|         - |  804 | `/*` |
|         - |  805 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  806 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  807 | ` *   Error: Object of class X could not be converted to string` |
|         - |  808 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  809 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  810 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  811 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  812 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  813 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  814 | ` * cast still carried the placeholder.` |
|         - |  815 | ` *` |
|         - |  816 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  817 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - |  818 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - |  819 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - |  820 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - |  821 | ` * settype() site then blanks its target itself, because php's` |
|         - |  822 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - |  823 | ` */` |
|       566 |  824 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         4 |  825 | `{` |
|       570 |  826 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - |  827 | `	SyBlob sMsg;` |
|       570 |  828 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       570 |  829 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       566 |  830 | `		&pInst->pClass->sName);` |
|         - |  831 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       570 |  832 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         4 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - |  836 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - |  837 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - |  838 | ` */` |
|     83926 |  839 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 |  840 | `{` |
|         - |  841 | `	ph7_class_instance *pInst;` |
|     83931 |  842 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|     81873 |  843 | `		return FALSE;` |
|         - |  844 | `	}` |
|      2063 |  845 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      2063 |  846 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 |  847 | `		return FALSE;` |
|         - |  848 | `	}` |
|      2063 |  849 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     41961 |  850 | `}` |
|         - |  851 | `/*` |
|         - |  852 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - |  853 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - |  854 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - |  855 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - |  856 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - |  857 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - |  858 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - |  859 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - |  860 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - |  861 | ` *` |
|         - |  862 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - |  863 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - |  864 | ` * ph7_value carries.` |
|         - |  865 | ` *` |
|         - |  866 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - |  867 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - |  868 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - |  869 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - |  870 | ` * exactly as php never throws for them.` |
|         - |  871 | ` *` |
|         - |  872 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - |  873 | ` */` |
|    985014 |  874 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 |  875 | `{` |
|    985019 |  876 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|    911751 |  877 | `		return SXRET_OK;` |
|         - |  878 | `	}` |
|     73273 |  879 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|        92 |  880 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|        44 |  881 | `	}` |
|         - |  882 | `	/* php 8.5's other coercion warning, and it rides HERE for the same reason` |
|         - |  883 | `	 * that one does: this is the conversion a program asked for -- a cast, echo,` |
|         - |  884 | ``	 * concatenation, interpolation, a `string` parameter -- and not the internal`` |
|         - |  885 | `	 * one a comparison or a debug renderer makes. A NaN is the only float that` |
|         - |  886 | `	 * warns; INF and -INF spell themselves out in silence. */` |
|     73273 |  887 | `	if( (pObj->iFlags & MEMOBJ_REAL) && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        17 |  888 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,` |
|         - |  889 | `			"unexpected NAN value was coerced to string");` |
|         8 |  890 | `	}` |
|     73273 |  891 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       570 |  892 | `		return MemObjThrowNotStringable(pObj);` |
|         - |  893 | `	}` |
|     72707 |  894 | `	return PH7_MemObjToString(pObj);` |
|    492507 |  895 | `}` |
|         - |  896 | `/*` |
|         - |  897 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - |  898 | ` * representation.` |
|         - |  899 | ` */` |
|         2 |  900 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 |  901 | `{` |
|         3 |  902 | `	return PH7_MemObjRelease(pObj);` |
|         1 |  903 | `}` |
|         - |  904 | `/*` |
|         - |  905 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - |  906 | `  * According to the PHP language reference manual.` |
|         - |  907 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  908 | `  *   to an array results in an array with a single element with index zero` |
|         - |  909 | `  *   and the value of the scalar which was converted.` |
|         - |  910 | `  */` |
|      5842 |  911 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 |  912 | `{` |
|      5847 |  913 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  914 | `		ph7_hashmap *pMap;` |
|         - |  915 | `		/* Allocate a new hashmap instance */` |
|      5239 |  916 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|      5239 |  917 | `		if( pMap == 0 ){` |
|       ! 0 |  918 | `			return SXERR_MEM;` |
|         - |  919 | `		}` |
|      5239 |  920 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - |  921 | `			/*` |
|         - |  922 | `			 * According to the PHP language reference manual.` |
|         - |  923 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - |  924 | `			 *   to an array results in an array with a single element with index zero` |
|         - |  925 | `			 *   and the value of the scalar which was converted.` |
|         - |  926 | `			 */` |
|       159 |  927 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       133 |  928 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       128 |  929 | `				if( pInst && pObj->pVm->pClosureClass` |
|       133 |  930 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|         - |  931 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|         - |  932 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|         - |  933 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|         - |  934 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|         - |  935 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|         3 |  936 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         2 |  937 | `				}else{` |
|         - |  938 | `					/* Object cast */` |
|       130 |  939 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|         - |  940 | `				}` |
|        69 |  941 | `			}else{` |
|         - |  942 | `				/* Insert a single element */` |
|        28 |  943 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - |  944 | `			}` |
|       159 |  945 | `			SyBlobRelease(&pObj->sBlob);` |
|        77 |  946 | `		}` |
|         - |  947 | `		/* Invalidate any prior representation */` |
|      5239 |  948 | `		PH7_MemObjRelease(pObj);` |
|      5239 |  949 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|      5239 |  950 | `		pObj->x.pOther = pMap;` |
|      2617 |  951 | `	}` |
|      5847 |  952 | `	return SXRET_OK;` |
|      2926 |  953 | `}` |
|         - |  954 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - |  955 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - |  956 | ` * matching PHP) and holding a copy of the value. */` |
|         - |  957 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       132 |  958 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         4 |  959 | `{` |
|       136 |  960 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - |  961 | `	ph7_value *pSlot;` |
|         - |  962 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - |  963 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - |  964 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - |  965 | `	 * safe to coerce in place. */` |
|       136 |  966 | `	PH7_MemObjToString(pKey);` |
|       202 |  967 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       132 |  968 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       136 |  969 | `	if( pSlot ){` |
|       136 |  970 | `		PH7_MemObjStore(pValue,pSlot);` |
|        66 |  971 | `	}` |
|       136 |  972 | `	return SXRET_OK;` |
|         4 |  973 | `}` |
|         - |  974 | `/*` |
|         - |  975 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - |  976 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - |  977 | ` * matching PHP's (object) cast:` |
|         - |  978 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - |  979 | ` *   - scalar -> a single property named "scalar".` |
|         - |  980 | ` *   - null   -> an empty stdClass (no properties).` |
|         - |  981 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - |  982 | ` */` |
|        88 |  983 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         4 |  984 | `{` |
|        92 |  985 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - |  986 | `		ph7_class_instance *pStd;` |
|         - |  987 | `		ph7_class *pClass;` |
|         - |  988 | `		ph7_vm *pVm;` |
|         - |  989 | `		/* Point to the underlying VM + the stdClass */` |
|        92 |  990 | `		pVm = pObj->pVm;` |
|       136 |  991 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        44 |  992 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|        92 |  993 | `		if( pClass == 0 ){` |
|         - |  994 | `			/* Can't happen,load null instead */` |
|       ! 0 |  995 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 |  996 | `			return SXRET_OK;` |
|         - |  997 | `		}` |
|         - |  998 | `		/* Instanciate a new (empty) stdClass object */` |
|        92 |  999 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|        92 | 1000 | `		if( pStd == 0 ){` |
|         - | 1001 | `			/* Out of memory */` |
|       ! 0 | 1002 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 | 1003 | `			return SXRET_OK;` |
|         - | 1004 | `		}` |
|        92 | 1005 | `		pStd->iRef = 1;` |
|        92 | 1006 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1007 | `			/* Array: one dynamic property per entry. */` |
|         - | 1008 | `			struct VmObjCastData sData;` |
|        78 | 1009 | `			sData.pVm = pVm;` |
|        78 | 1010 | `			sData.pStd = pStd;` |
|        78 | 1011 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        53 | 1012 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1013 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 | 1014 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 | 1015 | `			if( pSlot ){` |
|        14 | 1016 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 | 1017 | `			}` |
|         6 | 1018 | `		}` |
|         - | 1019 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - | 1020 | `		/* Invalidate any prior representation */` |
|        92 | 1021 | `		PH7_MemObjRelease(pObj);` |
|         - | 1022 | `		/* Save the new instance */` |
|        92 | 1023 | `		pObj->x.pOther = pStd;` |
|        92 | 1024 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        44 | 1025 | `	}` |
|        92 | 1026 | `	return SXRET_OK;` |
|        48 | 1027 | `}` |
|         - | 1028 | `/*` |
|         - | 1029 | ` * Return a pointer to the appropriate convertion method associated` |
|         - | 1030 | ` * with the given type.` |
|         - | 1031 | ` * Note on type juggling.` |
|         - | 1032 | ` * Accoding to the PHP language reference manual` |
|         - | 1033 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - | 1034 | ` *  declaration; a variable's type is determined by the context in which` |
|         - | 1035 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - | 1036 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - | 1037 | ` *  assigned to $var, it becomes an integer.` |
|         - | 1038 | ` */` |
|    100268 | 1039 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 | 1040 | `{` |
|    100273 | 1041 | `	if( iFlags & MEMOBJ_STRING ){` |
|       101 | 1042 | `		return PH7_MemObjToString;` |
|    100177 | 1043 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100113 | 1044 | `		return PH7_MemObjToInteger;` |
|        68 | 1045 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        55 | 1046 | `		return PH7_MemObjToReal;` |
|        15 | 1047 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         7 | 1048 | `		return PH7_MemObjToBool;` |
|         8 | 1049 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         8 | 1050 | `		return PH7_MemObjToHashmap;` |
|       ! 0 | 1051 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 1052 | `		return PH7_MemObjToObject;` |
|       ! 0 | 1053 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - | 1054 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - | 1055 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - | 1056 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - | 1057 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - | 1058 | `		 * default. */` |
|       ! 0 | 1059 | `		return 0;` |
|         - | 1060 | `	}` |
|         - | 1061 | `	/* NULL cast */` |
|       ! 0 | 1062 | `	return PH7_MemObjToNull;` |
|     50139 | 1063 | `}` |
|         - | 1064 | `/*` |
|         - | 1065 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - | 1066 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - | 1067 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - | 1068 | ` * loose-comparison numeric gate:` |
|         - | 1069 | ` *` |
|         - | 1070 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - | 1071 | ` *` |
|         - | 1072 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - | 1073 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - | 1074 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - | 1075 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - | 1076 | ` * a non-string value.` |
|         - | 1077 | ` */` |
|         - | 1078 | `/*` |
|         - | 1079 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - | 1080 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - | 1081 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - | 1082 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - | 1083 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - | 1084 | ` * and rejects a string with no prefix outright.` |
|         - | 1085 | ` */` |
|    424049 | 1086 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 | 1087 | `{` |
|         - | 1088 | `	const char *z, *zEnd;` |
|         - | 1089 | `	sxu32 n;` |
|    424054 | 1090 | `	int bDigit = 0;` |
|    424054 | 1091 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 1092 | `		return 0;` |
|         - | 1093 | `	}` |
|    424054 | 1094 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|    424054 | 1095 | `	n = SyBlobLength(&pValue->sBlob);` |
|    424054 | 1096 | `	if( n == 0 ){` |
|       202 | 1097 | `		return 0;` |
|         - | 1098 | `	}` |
|    423856 | 1099 | `	zEnd = z + n;` |
|    424234 | 1100 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|       382 | 1101 | `		z++;` |
|         4 | 1102 | `	}` |
|    423856 | 1103 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       779 | 1104 | `		z++;` |
|       387 | 1105 | `	}` |
|    524592 | 1106 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|    100741 | 1107 | `		z++; bDigit = 1;` |
|         5 | 1108 | `	}` |
|    423856 | 1109 | `	if( z < zEnd && z[0] == '.' ){` |
|      6010 | 1110 | `		z++;` |
|      7510 | 1111 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1505 | 1112 | `			z++; bDigit = 1;` |
|         5 | 1113 | `		}` |
|      3169 | 1114 | `	}` |
|         - | 1115 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|    423856 | 1116 | `	if( !bDigit ){` |
|    416406 | 1117 | `		return 0;` |
|         - | 1118 | `	}` |
|         - | 1119 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - | 1120 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|      7455 | 1121 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       332 | 1122 | `		const char *zExp = z;` |
|       332 | 1123 | `		z++;` |
|       332 | 1124 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|        23 | 1125 | `			z++;` |
|        11 | 1126 | `		}` |
|       332 | 1127 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        20 | 1128 | `			z = zExp;` |
|        11 | 1129 | `		}else{` |
|       742 | 1130 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       432 | 1131 | `				z++;` |
|         4 | 1132 | `			}` |
|         - | 1133 | `		}` |
|       164 | 1134 | `	}` |
|      7455 | 1135 | `	if( pzTail ){` |
|      7415 | 1136 | `		*pzTail = z;` |
|      3680 | 1137 | `	}` |
|      7455 | 1138 | `	return 1;` |
|    211869 | 1139 | `}` |
|         - | 1140 | `/*` |
|         - | 1141 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - | 1142 | ` * (trailing whitespace allowed, nothing else).` |
|         - | 1143 | ` */` |
|    418643 | 1144 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 | 1145 | `{` |
|    418648 | 1146 | `	const char *zTail = 0, *zEnd;` |
|    418648 | 1147 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    416426 | 1148 | `		return 0;` |
|         - | 1149 | `	}` |
|      2227 | 1150 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      2271 | 1151 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        46 | 1152 | `		zTail++;` |
|         2 | 1153 | `	}` |
|      2227 | 1154 | `	return zTail == zEnd ? 1 : 0;` |
|    209182 | 1155 | `}` |
|         - | 1156 | `/*` |
|         - | 1157 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - | 1158 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - | 1159 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - | 1160 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - | 1161 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - | 1162 | ` * those bytes convert to when it did.` |
|         - | 1163 | ` *` |
|         - | 1164 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - | 1165 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - | 1166 | ` * however large it is, because it was always going to be a double, so making it` |
|         - | 1167 | ` * one lost no digits.` |
|         - | 1168 | ` *` |
|         - | 1169 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - | 1170 | ` * intact when this says no.` |
|         - | 1171 | ` */` |
|       852 | 1172 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         3 | 1173 | `{` |
|       855 | 1174 | `	const char *zTail = 0;` |
|       855 | 1175 | `	int iOverflow = 0;` |
|       855 | 1176 | `	*piOverflow = 0;` |
|       855 | 1177 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|       104 | 1178 | `		return FALSE;` |
|         - | 1179 | `	}` |
|       753 | 1180 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 | 1181 | `		return FALSE;` |
|         - | 1182 | `	}` |
|         - | 1183 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - | 1184 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|       753 | 1185 | `	if( MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|        82 | 1186 | `		return FALSE;` |
|         - | 1187 | `	}` |
|       673 | 1188 | `	MemObjStringToInt(pObj,&iOverflow);` |
|       673 | 1189 | `	*piOverflow = iOverflow;` |
|       673 | 1190 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1191 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1192 | `	}` |
|       673 | 1193 | `	return TRUE;` |
|       425 | 1194 | `}` |
|         - | 1195 | `/*` |
|         - | 1196 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1197 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1198 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1199 | ` */` |
|    324253 | 1200 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1201 | `{` |
|    324258 | 1202 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      1931 | 1203 | `		return TRUE;` |
|    322332 | 1204 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      5017 | 1205 | `		return FALSE;` |
|    317320 | 1206 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1207 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    317320 | 1208 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1209 | `	}` |
|         - | 1210 | `	/* NOT REACHED */` |
|       ! 0 | 1211 | `	return FALSE;` |
|    161991 | 1212 | `}` |
|         - | 1213 | `/*` |
|         - | 1214 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1215 | ` * FALSE otherwise.` |
|         - | 1216 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1217 | ` * NULL value.` |
|         - | 1218 | ` * Boolean FALSE.` |
|         - | 1219 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1220 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1221 | ` * An empty array.` |
|         - | 1222 | ` * NOTE` |
|         - | 1223 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1224 | ` */` |
|     49510 | 1225 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1226 | `{` |
|         - | 1227 | ``	/* php's `empty($x)` is `!zend_is_true($x)` -- the same question the bool cast`` |
|         - | 1228 | `	 * asks, and this used to answer it with rules of its own. They disagreed on a` |
|         - | 1229 | ``	 * string of MORE THAN ONE zero: the old walk called every `"0"` run empty, so`` |
|         - | 1230 | ``	 * `empty("00")` was true and `array_filter(["00"])` dropped it, where php`` |
|         - | 1231 | ``	 * keeps both (only `""` and the single byte `"0"` are false there). The`` |
|         - | 1232 | `	 * warning php raises when a NaN is coerced rides the same door, since` |
|         - | 1233 | ``	 * `empty(NAN)` warns there. */`` |
|     49510 | 1234 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     24762 | 1235 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         5 | 1236 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 1237 | `			"unexpected NAN value was coerced to bool");` |
|         2 | 1238 | `	}` |
|     49515 | 1239 | `	return !MemObjIsTruthy(&(*pObj));` |
|         5 | 1240 | `}` |
|         - | 1241 | `/*` |
|         - | 1242 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1243 | ` * or both.` |
|         - | 1244 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1245 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1246 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1247 | ` * and ignore the rest.` |
|         - | 1248 | ` */` |
|    939977 | 1249 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1250 | `{` |
|    939982 | 1251 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|    938944 | 1252 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|        80 | 1253 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|        61 | 1254 | `				pObj->x.iVal = 0;` |
|        29 | 1255 | `			}` |
|        80 | 1256 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|        38 | 1257 | `		}` |
|         - | 1258 | `		/* Already numeric */` |
|    938944 | 1259 | `		return  SXRET_OK;` |
|         - | 1260 | `	}` |
|      1043 | 1261 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      1043 | 1262 | `		const char *zTail = 0;` |
|      1043 | 1263 | `		int bNum, bReal = 0;` |
|         - | 1264 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1265 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1266 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1267 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      1043 | 1268 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      1043 | 1269 | `		if( bNum ){` |
|      1011 | 1270 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      7299 | 1271 | `			while( z < zTail ){` |
|      6479 | 1272 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       190 | 1273 | `					bReal = 1;` |
|       190 | 1274 | `					break;` |
|         - | 1275 | `				}` |
|      6293 | 1276 | `				z++;` |
|         5 | 1277 | `			}` |
|       499 | 1278 | `		}` |
|      1043 | 1279 | `		if( bReal ){` |
|       190 | 1280 | `			PH7_MemObjToReal(&(*pObj));` |
|        97 | 1281 | `		}else{` |
|       857 | 1282 | `			if( !bNum ){` |
|         - | 1283 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        33 | 1284 | `				pObj->x.iVal = 0;` |
|        17 | 1285 | `			}else{` |
|       825 | 1286 | `				int iOverflow = 0;` |
|         - | 1287 | `				/* Convert as much as we can */` |
|       825 | 1288 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1289 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       825 | 1290 | `				if( iOverflow ){` |
|         - | 1291 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1292 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1293 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1294 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1295 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1296 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1297 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1298 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1299 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1300 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1301 | `					 * OP_ADD's overflow arm makes there. */` |
|       228 | 1302 | `					PH7_MemObjToReal(&(*pObj));` |
|       228 | 1303 | `					return SXRET_OK;` |
|         - | 1304 | `				}` |
|         - | 1305 | `#endif` |
|         - | 1306 | `			}` |
|       631 | 1307 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       631 | 1308 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1309 | `		}` |
|       402 | 1310 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|       ! 0 | 1311 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1312 | `	}else{` |
|         - | 1313 | `		/* Perform a blind cast */` |
|       ! 0 | 1314 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1315 | `	}` |
|       817 | 1316 | `	return SXRET_OK;` |
|    470756 | 1317 | `}` |
|         - | 1318 | `/*` |
|         - | 1319 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1320 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1321 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1322 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1323 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1324 | ` * last carried character. Empty strings become "1".` |
|         - | 1325 | ` *` |
|         - | 1326 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1327 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1328 | ` * a string even though it looks numeric.` |
|         - | 1329 | ` */` |
|       ! 0 | 1330 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1331 | `{` |
|         - | 1332 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1333 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1334 | `	sxu32 nLen, pos;` |
|         - | 1335 | `	sxu8 *zStr;` |
|       ! 0 | 1336 | `	int carry = 1;` |
|         - | 1337 | `	int ch;` |
|         - | 1338 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1339 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1340 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1341 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1342 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1343 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1344 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1345 | `	}` |
|       ! 0 | 1346 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1347 | `	if( nLen == 0 ){` |
|       ! 0 | 1348 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1349 | `		return SXRET_OK;` |
|         - | 1350 | `	}` |
|       ! 0 | 1351 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1352 | `	pos = nLen;` |
|       ! 0 | 1353 | `	while( pos > 0 ){` |
|       ! 0 | 1354 | `		pos--;` |
|       ! 0 | 1355 | `		ch = zStr[pos];` |
|       ! 0 | 1356 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1357 | `			if( ch == 'z' ){` |
|       ! 0 | 1358 | `				zStr[pos] = 'a';` |
|       ! 0 | 1359 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1360 | `				continue;` |
|         - | 1361 | `			}` |
|       ! 0 | 1362 | `			zStr[pos]++;` |
|       ! 0 | 1363 | `			carry = 0;` |
|       ! 0 | 1364 | `			break;` |
|       ! 0 | 1365 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1366 | `			if( ch == 'Z' ){` |
|       ! 0 | 1367 | `				zStr[pos] = 'A';` |
|       ! 0 | 1368 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1369 | `				continue;` |
|         - | 1370 | `			}` |
|       ! 0 | 1371 | `			zStr[pos]++;` |
|       ! 0 | 1372 | `			carry = 0;` |
|       ! 0 | 1373 | `			break;` |
|       ! 0 | 1374 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1375 | `			if( ch == '9' ){` |
|       ! 0 | 1376 | `				zStr[pos] = '0';` |
|       ! 0 | 1377 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1378 | `				continue;` |
|         - | 1379 | `			}` |
|       ! 0 | 1380 | `			zStr[pos]++;` |
|       ! 0 | 1381 | `			carry = 0;` |
|       ! 0 | 1382 | `			break;` |
|       ! 0 | 1383 | `		}else{` |
|         - | 1384 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1385 | `			carry = 0;` |
|       ! 0 | 1386 | `			break;` |
|         - | 1387 | `		}` |
|       ! 0 | 1388 | `	}` |
|       ! 0 | 1389 | `	if( carry ){` |
|         - | 1390 | `		sxu8 prepend;` |
|         - | 1391 | `		sxu32 i;` |
|       ! 0 | 1392 | `		switch( last_class ){` |
|       ! 0 | 1393 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1394 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1395 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1396 | `		}` |
|         - | 1397 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1398 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1399 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1400 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1401 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1402 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1403 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1404 | `		}` |
|       ! 0 | 1405 | `		zStr[0] = prepend;` |
|       ! 0 | 1406 | `	}` |
|       ! 0 | 1407 | `	return SXRET_OK;` |
|       ! 0 | 1408 | `}` |
|         - | 1409 | `/*` |
|         - | 1410 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1411 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1412 | ` */` |
|      3022 | 1413 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         4 | 1414 | `{` |
|      3026 | 1415 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1416 | `		/* Work only with reals */` |
|      3026 | 1417 | `		MemObjTryIntger(&(*pObj));` |
|      1511 | 1418 | `	}` |
|      3026 | 1419 | `	return SXRET_OK;` |
|         4 | 1420 | `}` |
|         - | 1421 | `/*` |
|         - | 1422 | ` * Initialize a ph7_value to the null type.` |
|         - | 1423 | ` */` |
| 102242779 | 1424 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1425 | `{` |
|         - | 1426 | `	/* Zero the structure */` |
| 102242784 | 1427 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1428 | `	/* Initialize fields */` |
| 102242784 | 1429 | `	pObj->pVm = pVm;` |
| 102242784 | 1430 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1431 | `	/* Set the NULL type */` |
| 102242784 | 1432 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 102242784 | 1433 | `	return SXRET_OK;` |
|         5 | 1434 | `}` |
|         - | 1435 | `/*` |
|         - | 1436 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1437 | ` */` |
|   7503699 | 1438 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1439 | `{` |
|         - | 1440 | `	/* Zero the structure */` |
|   7503704 | 1441 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1442 | `	/* Initialize fields */` |
|   7503704 | 1443 | `	pObj->pVm = pVm;` |
|   7503704 | 1444 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1445 | `	/* Set the desired type */` |
|   7503704 | 1446 | `	pObj->x.iVal = iVal;` |
|   7503704 | 1447 | `	pObj->iFlags = MEMOBJ_INT;` |
|   7503704 | 1448 | `	return SXRET_OK;` |
|         5 | 1449 | `}` |
|         - | 1450 | `/*` |
|         - | 1451 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1452 | ` */` |
|     26768 | 1453 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1454 | `{` |
|         - | 1455 | `	/* Zero the structure */` |
|     26773 | 1456 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1457 | `	/* Initialize fields */` |
|     26773 | 1458 | `	pObj->pVm = pVm;` |
|     26773 | 1459 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1460 | `	/* Set the desired type */` |
|     26773 | 1461 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     26773 | 1462 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     26773 | 1463 | `	return SXRET_OK;` |
|         5 | 1464 | `}` |
|         - | 1465 | `/*` |
|         - | 1466 | ` * Initialize a ph7_value to the real type.` |
|         - | 1467 | ` */` |
|       770 | 1468 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         3 | 1469 | `{` |
|         - | 1470 | `	/* Zero the structure */` |
|       773 | 1471 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1472 | `	/* Initialize fields */` |
|       773 | 1473 | `	pObj->pVm = pVm;` |
|       773 | 1474 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1475 | `	/* Set the desired type */` |
|       773 | 1476 | `	pObj->rVal = rVal;` |
|       773 | 1477 | `	pObj->iFlags = MEMOBJ_REAL;` |
|       773 | 1478 | `	return SXRET_OK;` |
|         3 | 1479 | `}` |
|         - | 1480 | `/*` |
|         - | 1481 | ` * Initialize a ph7_value to the array type.` |
|         - | 1482 | ` */` |
|   3625372 | 1483 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1484 | `{` |
|         - | 1485 | `	/* Zero the structure */` |
|   3625377 | 1486 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1487 | `	/* Initialize fields */` |
|   3625377 | 1488 | `	pObj->pVm = pVm;` |
|   3625377 | 1489 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1490 | `	/* Set the desired type */` |
|   3625377 | 1491 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   3625377 | 1492 | `	pObj->x.pOther = pArray;` |
|   3625377 | 1493 | `	return SXRET_OK;` |
|         5 | 1494 | `}` |
|         - | 1495 | `/*` |
|         - | 1496 | ` * Initialize a ph7_value to the string type.` |
|         - | 1497 | ` */` |
|  11653277 | 1498 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1499 | `{` |
|         - | 1500 | `	/* Zero the structure */` |
|  11653282 | 1501 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1502 | `	/* Initialize fields */` |
|  11653282 | 1503 | `	pObj->pVm = pVm;` |
|  11653282 | 1504 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  11653282 | 1505 | `	if( pVal ){` |
|         - | 1506 | `		/* Append contents */` |
|   8225959 | 1507 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   4112970 | 1508 | `	}` |
|         - | 1509 | `	/* Set the desired type */` |
|  11653282 | 1510 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  11653282 | 1511 | `	return SXRET_OK;` |
|         5 | 1512 | `}` |
|         - | 1513 | `/*` |
|         - | 1514 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1515 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1516 | ` * invalidate any prior representation and set the string type.` |
|         - | 1517 | ` * Then a simple append operation is performed.` |
|         - | 1518 | ` */` |
|   3976058 | 1519 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1520 | `{` |
|         - | 1521 | `	sxi32 rc;` |
|   3976063 | 1522 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1523 | `		/* Invalidate any prior representation */` |
|     34924 | 1524 | `		PH7_MemObjRelease(pObj);` |
|     34924 | 1525 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     17455 | 1526 | `	}` |
|         - | 1527 | `	/* Append contents */` |
|   3976063 | 1528 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|   3976063 | 1529 | `	return rc;` |
|         5 | 1530 | `}` |
|         - | 1531 | `#if 0` |
|         - | 1532 | `/*` |
|         - | 1533 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1534 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1535 | ` * any prior representation and set the string type.` |
|         - | 1536 | ` * Then a simple format and append operation is performed.` |
|         - | 1537 | ` */` |
|         - | 1538 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1539 | `{` |
|         - | 1540 | `	sxi32 rc;` |
|         - | 1541 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1542 | `		/* Invalidate any prior representation */` |
|         - | 1543 | `		PH7_MemObjRelease(pObj);` |
|         - | 1544 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1545 | `	}` |
|         - | 1546 | `	/* Format and append contents */` |
|         - | 1547 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1548 | `	return rc;` |
|         - | 1549 | `}` |
|         - | 1550 | `#endif` |
|         - | 1551 | `/*` |
|         - | 1552 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1553 | ` */` |
|  17477608 | 1554 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1555 | `{` |
|  17477613 | 1556 | `	ph7_class_instance *pObj = 0;` |
|  17477613 | 1557 | `	ph7_hashmap *pMap = 0;` |
|         - | 1558 | `	sxi32 rc;` |
|  17477613 | 1559 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1560 | `		/* Increment reference count */` |
|   2362270 | 1561 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  16296477 | 1562 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1563 | `		/* Increment reference count */` |
|     78073 | 1564 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|     39025 | 1565 | `	}` |
|  17477613 | 1566 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    103663 | 1567 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  17425783 | 1568 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     19507 | 1569 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|      9751 | 1570 | `	}` |
|  17477613 | 1571 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|  17477613 | 1572 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  17477613 | 1573 | `	rc = SXRET_OK;` |
|  17477613 | 1574 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  10561640 | 1575 | `		SyBlobReset(&pDest->sBlob);` |
|  10561640 | 1576 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|   5281991 | 1577 | `	}else{` |
|   6915978 | 1578 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   1943927 | 1579 | `			SyBlobRelease(&pDest->sBlob);` |
|    972573 | 1580 | `		}` |
|         - | 1581 | `	}` |
|  17477613 | 1582 | `	if( pMap ){` |
|    103663 | 1583 | `		PH7_HashmapUnref(pMap);` |
|  17425783 | 1584 | `	}else if( pObj ){` |
|     19507 | 1585 | `		PH7_ClassInstanceUnref(pObj);` |
|      9751 | 1586 | `	}` |
|  17477608 | 1587 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|   9923152 | 1588 | `	 && pDest->pVm` |
|   2362265 | 1589 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1590 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1591 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1592 | `	  * for closure envs and other non-slot destinations. */` |
|   1181138 | 1593 | `	 && pDest != (ph7_value *)SySetAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1594 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1595 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1596 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1597 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1598 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1599 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1600 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1601 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1602 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1603 | `			pDest->x.pOther = pSnap;` |
|         4 | 1604 | `		}else if( pSnap ){` |
|       ! 0 | 1605 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1606 | `		}` |
|         4 | 1607 | `	}` |
|  17477613 | 1608 | `	return rc;` |
|         5 | 1609 | `}` |
|         - | 1610 | `/*` |
|         - | 1611 | ` * Duplicate the contents of a ph7_value but do not copy internal` |
|         - | 1612 | ` * buffer contents,simply point to it.` |
|         - | 1613 | ` */` |
|  19275800 | 1614 | `PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1615 | `{` |
|  19275805 | 1616 | `	SyMemcpy((const void *)&(*pSrc),&(*pDest),` |
|         - | 1617 | `		sizeof(ph7_value)-(sizeof(ph7_vm *)+sizeof(SyBlob)+sizeof(sxu32)));` |
|         - | 1618 | `	/* D1 commit 2: a MEMOBJ_AUX_DEFPATH carrier OWNS its heap descriptor via x.pOther, and` |
|         - | 1619 | `	 * PH7_MemObjRelease frees it exactly once. An aliasing Load copies iFlags+x.pOther` |
|         - | 1620 | `	 * verbatim, so a Load-duplicated carrier would let two slots free the same descriptor.` |
|         - | 1621 | `	 * Carriers are transient (produced by LOAD_IDX/MEMBER, consumed at OP_CALL) and are never` |
|         - | 1622 | `	 * Load-copied today; strip the flag defensively so the invariant can't be violated. */` |
|  19275805 | 1623 | `	pDest->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|  19275805 | 1624 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1625 | `		/* Increment reference count */` |
|    864375 | 1626 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  18843614 | 1627 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1628 | `		/* Increment reference count */` |
|    469546 | 1629 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    234772 | 1630 | `	}` |
|  19275805 | 1631 | `	if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|       209 | 1632 | `		SyBlobRelease(&pDest->sBlob);` |
|       102 | 1633 | `	}` |
|  19275805 | 1634 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  11221981 | 1635 | `		SyBlobReadOnly(&pDest->sBlob,SyBlobData(&pSrc->sBlob),SyBlobLength(&pSrc->sBlob));` |
|   5615076 | 1636 | `	}` |
|  19275805 | 1637 | `	return SXRET_OK;` |
|         5 | 1638 | `}` |
|         - | 1639 | `/*` |
|         - | 1640 | ` * Read a value WITHOUT converting the caller's copy of it.` |
|         - | 1641 | ` *` |
|         - | 1642 | ` * Every ph7_value_to_xxx()/PH7_MemObjToXxx() is destructive: it rewrites the` |
|         - | 1643 | ` * object it is handed and throws the prior representation away. That is right` |
|         - | 1644 | ` * for a VM operand, and wrong for an entry a builtin FETCHED out of an array` |
|         - | 1645 | ` * the script still holds — an $options member, a stream-filter parameter, a` |
|         - | 1646 | ` * proc_open descriptor — where converting in place rewrites the script's own` |
|         - | 1647 | ` * array (php's zval_get_long()/zval_get_string() family never touch theirs).` |
|         - | 1648 | ` *` |
|         - | 1649 | ` * PH7_ValuePeek loads an aliasing copy into pScratch (which the caller must` |
|         - | 1650 | ` * have PH7_MemObjInit'd and must PH7_MemObjRelease afterwards) and answers it,` |
|         - | 1651 | ` * so the destructive conversion lands on the copy. A string read through it` |
|         - | 1652 | ` * stays valid until the scratch value is released. The three scalar wrappers` |
|         - | 1653 | ` * carry their own scratch for the common case.` |
|         - | 1654 | ` */` |
|       154 | 1655 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch)` |
|         3 | 1656 | `{` |
|       157 | 1657 | `	PH7_MemObjLoad(pVal,pScratch);` |
|       157 | 1658 | `	return pScratch;` |
|         3 | 1659 | `}` |
|      1566 | 1660 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal)` |
|         5 | 1661 | `{` |
|         - | 1662 | `	ph7_value sTmp;` |
|         - | 1663 | `	sxi64 iVal;` |
|      1571 | 1664 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|      1571 | 1665 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|      1571 | 1666 | `	PH7_MemObjToInteger(&sTmp);` |
|      1571 | 1667 | `	iVal = sTmp.x.iVal;` |
|      1571 | 1668 | `	PH7_MemObjRelease(&sTmp);` |
|      1571 | 1669 | `	return iVal;` |
|         5 | 1670 | `}` |
|         - | 1671 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       506 | 1672 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal)` |
|         3 | 1673 | `{` |
|         - | 1674 | `	ph7_value sTmp;` |
|         - | 1675 | `	ph7_real rVal;` |
|       509 | 1676 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|       509 | 1677 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|       509 | 1678 | `	PH7_MemObjToReal(&sTmp);` |
|       509 | 1679 | `	rVal = sTmp.rVal;` |
|       509 | 1680 | `	PH7_MemObjRelease(&sTmp);` |
|       509 | 1681 | `	return rVal;` |
|         3 | 1682 | `}` |
|         - | 1683 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         6 | 1684 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal)` |
|         2 | 1685 | `{` |
|         - | 1686 | `	ph7_value sTmp;` |
|         - | 1687 | `	int bVal;` |
|         8 | 1688 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|         8 | 1689 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|         8 | 1690 | `	PH7_MemObjToBool(&sTmp);` |
|         8 | 1691 | `	bVal = sTmp.x.iVal != 0;` |
|         8 | 1692 | `	PH7_MemObjRelease(&sTmp);` |
|         8 | 1693 | `	return bVal;` |
|         2 | 1694 | `}` |
|         - | 1695 | `/*` |
|         - | 1696 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1697 | ` */` |
| 127114427 | 1698 | `PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj)` |
|         5 | 1699 | `{` |
| 127114432 | 1700 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1701 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1702 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1703 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1704 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1705 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1706 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1707 | `		pObj->x.pOther = 0;` |
|         7 | 1708 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1709 | `	}` |
| 127114432 | 1710 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - | 1711 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|         - | 1712 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|         - | 1713 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|         - | 1714 | `		 * argument list threw never leaks the receiver it was holding. */` |
|       ! 0 | 1715 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|       ! 0 | 1716 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1717 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       ! 0 | 1718 | `	}` |
| 127114432 | 1719 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1720 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1721 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1722 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1723 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|         3 | 1724 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|         3 | 1725 | `		pObj->x.pOther = 0;` |
|         3 | 1726 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|         1 | 1727 | `	}` |
| 127114432 | 1728 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|  63372434 | 1729 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|   6629138 | 1730 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|  60057857 | 1731 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   5170977 | 1732 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|   2585478 | 1733 | `		}` |
|         - | 1734 | `		/* Release the internal buffer */` |
|  63372434 | 1735 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1736 | `		/* Invalidate any prior representation */` |
|  63372434 | 1737 | `		pObj->iFlags = MEMOBJ_NULL;` |
|  31699435 | 1738 | `	}` |
| 127114432 | 1739 | `	return SXRET_OK;` |
|         5 | 1740 | `}` |
|         - | 1741 | `/*` |
|         - | 1742 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1743 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1744 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1745 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1746 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1747 | ` *` |
|         - | 1748 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1749 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1750 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1751 | ` *` |
|         - | 1752 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1753 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1754 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1755 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1756 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1757 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1758 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1759 | ` */` |
|       206 | 1760 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         3 | 1761 | `{` |
|       209 | 1762 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       209 | 1763 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       209 | 1764 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|       150 | 1765 | `		if( PH7_MemObjIsNotStringable(pSelf)` |
|       140 | 1766 | `		 \|\| (pSelf->pVm && PH7_CALLBACK_UNWOUND(pSelf->pVm->nBoundaryRc)) ){` |
|         - | 1767 | `			/* php enters no PHP function while an exception is pending` |
|         - | 1768 | `			 * (zend_call_function bails on EG(exception)), so a __toString()` |
|         - | 1769 | `			 * that already threw -- or exited -- is NOT run again: every later` |
|         - | 1770 | `			 * comparison orders this operand the way a refused cast does. A sort` |
|         - | 1771 | `			 * used to re-enter the body once per remaining pair, and where the` |
|         - | 1772 | `			 * enclosing catch had already run in place the second throw was` |
|         - | 1773 | `			 * UNCAUGHT and killed the script. */` |
|        62 | 1774 | `			return FALSE;` |
|         - | 1775 | `		}` |
|        91 | 1776 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        91 | 1777 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1778 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1779 | `			 * point; until then order the operands the way a refused cast does. */` |
|        39 | 1780 | `			return FALSE;` |
|         - | 1781 | `		}` |
|        53 | 1782 | `		return TRUE;` |
|         - | 1783 | `	}` |
|        58 | 1784 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1785 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|         7 | 1786 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|         7 | 1787 | `		return TRUE;` |
|         - | 1788 | `	}` |
|        52 | 1789 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1790 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1791 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1792 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1793 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1794 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|        10 | 1795 | `		}` |
|        21 | 1796 | `		if( bReal ){` |
|         7 | 1797 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 1798 | `		}else{` |
|        15 | 1799 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 1800 | `		}` |
|        21 | 1801 | `		return TRUE;` |
|         - | 1802 | `	}` |
|        32 | 1803 | `	return FALSE;` |
|       106 | 1804 | `}` |
|         - | 1805 | `/*` |
|         - | 1806 | ` * Compare two ph7_values.` |
|         - | 1807 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 1808 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 1809 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 1810 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 1811 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 1812 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1813 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1814 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1815 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1816 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1817 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1818 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1819 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1820 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1821 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1822 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1823 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1824 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 1825 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1826 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1827 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1828 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 1829 | ` *      Loose comparisons with ==` |
|         - | 1830 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1831 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1832 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1833 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1834 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 1835 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1836 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1837 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1838 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1839 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 1840 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 1841 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1842 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 1843 | ` *    Strict comparisons with ===` |
|         - | 1844 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 1845 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1846 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1847 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1848 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1849 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1850 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1851 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1852 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 1853 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 1854 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 1855 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 1856 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 1857 | ` */` |
|   2563333 | 1858 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 1859 | `{` |
|         - | 1860 | `	sxi32 iComb;` |
|         - | 1861 | `	sxi32 rc;` |
|   2563338 | 1862 | `	if( bStrict ){` |
|         - | 1863 | `		sxi32 iF1,iF2;` |
|         - | 1864 | `		/* Strict comparisons with === */` |
|   1394179 | 1865 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   1394179 | 1866 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|   1394179 | 1867 | `		if( iF1 != iF2 ){` |
|         - | 1868 | `			/* Not of the same type */` |
|    347183 | 1869 | `			return 1;` |
|         - | 1870 | `		}` |
|    524377 | 1871 | `	}` |
|         - | 1872 | `	/* Combine flag together */` |
|   2216160 | 1873 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|   2216155 | 1874 | `	if( !bStrict` |
|   1693536 | 1875 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|    585361 | 1876 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        83 | 1877 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 1878 | `		/*` |
|         - | 1879 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 1880 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 1881 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 1882 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 1883 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 1884 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 1885 | `		 */` |
|        45 | 1886 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        31 | 1887 | `			PH7_MemObjToString(pObj1);` |
|        16 | 1888 | `		}else{` |
|        15 | 1889 | `			PH7_MemObjToString(pObj2);` |
|         - | 1890 | `		}` |
|        45 | 1891 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 1892 | `	}` |
|   2216160 | 1893 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 1894 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 1895 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        65 | 1896 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        65 | 1897 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        65 | 1898 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 1899 | `	}` |
|   2216098 | 1900 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 1901 | `		/*` |
|         - | 1902 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 1903 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 1904 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 1905 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 1906 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 1907 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 1908 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 1909 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 1910 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 1911 | `		 * equalled every open resource.` |
|         - | 1912 | `		 *` |
|         - | 1913 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 1914 | `		 * already answered 1.` |
|         - | 1915 | `		 */` |
|       209 | 1916 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       209 | 1917 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       209 | 1918 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 1919 | `		ph7_value sCast;` |
|       209 | 1920 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 1921 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        74 | 1922 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        44 | 1923 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|        79 | 1924 | `			PH7_MemObjRelease(&sCast);` |
|        79 | 1925 | `			return rc;` |
|         - | 1926 | `		}` |
|       131 | 1927 | `		PH7_MemObjRelease(&sCast);` |
|         - | 1928 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|       131 | 1929 | `		return bObj1 ? 1 : -1;` |
|         - | 1930 | `	}` |
|   2215892 | 1931 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|         - | 1932 | `		/* Convert to boolean: Keep in mind FALSE < TRUE. php decides null and bool` |
|         - | 1933 | `		 * this way and nothing else -- a RESOURCE used to be decided here too,` |
|         - | 1934 | `		 * which made every open one equal to every other truthy value. */` |
|     59501 | 1935 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     35129 | 1936 | `			MemObjToBoolQuiet(pObj1);   /* php's comparison says nothing about a NaN */` |
|     17560 | 1937 | `		}` |
|     59501 | 1938 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     33815 | 1939 | `			MemObjToBoolQuiet(pObj2);` |
|     16904 | 1940 | `		}` |
|     59501 | 1941 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|   2156396 | 1942 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 1943 | `		/* Hashmap aka 'array' comparison */` |
|       345 | 1944 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1945 | `			/* Array is always greater */` |
|        37 | 1946 | `			return -1;` |
|         - | 1947 | `		}` |
|       309 | 1948 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1949 | `			/* Array is always greater */` |
|        21 | 1950 | `			return 1;` |
|         - | 1951 | `		}` |
|         - | 1952 | `		/* Perform the comparison */` |
|       289 | 1953 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|       289 | 1954 | `		return rc;` |
|   2156056 | 1955 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 1956 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 1957 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 1958 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 1959 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|       641 | 1960 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1961 | `			/* Object is always greater */` |
|       ! 0 | 1962 | `			return -1;` |
|         - | 1963 | `		}` |
|       641 | 1964 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1965 | `			/* Object is always greater */` |
|       ! 0 | 1966 | `			return 1;` |
|         - | 1967 | `		}` |
|         - | 1968 | `		/* Perform the comparison */` |
|       641 | 1969 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|       641 | 1970 | `		return rc;` |
|   2155420 | 1971 | `	}else if( !VmIsUnorderedCmp(pObj1,pObj2) && (iComb & MEMOBJ_RES) ){` |
|         - | 1972 | `		/* php compares a resource with a NON-resource as its ID -- the number` |
|         - | 1973 | ``		 * `(int)$fp` answers -- and the other side takes php's LEGACY`` |
|         - | 1974 | `		 * scalar-to-number conversion, not php 8's saner string rule, because` |
|         - | 1975 | ``		 * this comparison never reaches that rule: `$fp == "5abc"` is TRUE for`` |
|         - | 1976 | ``		 * resource #5 and `$fp > "x"` compares 5 with 0. PHL compared the pair as`` |
|         - | 1977 | `		 * BOOLEANS, so an open resource equalled every non-empty string, every` |
|         - | 1978 | `		 * non-zero number and every other open resource, was GREATER than the` |
|         - | 1979 | ``		 * empty array, and `max($fp, 10)` answered the resource.`` |
|         - | 1980 | `		 *` |
|         - | 1981 | `		 * The two-resource case is decided above (by ID), null and bool before` |
|         - | 1982 | `		 * that (php's bool comparison), an array above this (an array is` |
|         - | 1983 | `		 * greater), an object by the cast rule, and a NaN by the unordered one --` |
|         - | 1984 | `		 * exactly php's order. */` |
|       229 | 1985 | `		int bRes1 = (pObj1->iFlags & MEMOBJ_RES) != 0;` |
|       229 | 1986 | `		ph7_value *pRes = bRes1 ? pObj1 : pObj2;` |
|       229 | 1987 | `		ph7_value *pOther = bRes1 ? pObj2 : pObj1;` |
|       229 | 1988 | `		sxi64 iId = (sxi64)PH7_VmResourceId(pRes->pVm,pRes->x.pOther);` |
|       229 | 1989 | `		PH7_MemObjToNumeric(pOther);` |
|       229 | 1990 | `		if( pOther->iFlags & MEMOBJ_REAL ){` |
|        57 | 1991 | `			ph7_real rId = (ph7_real)iId;` |
|        57 | 1992 | `			rc = rId > pOther->rVal ? 1 : (rId < pOther->rVal ? -1 : 0);` |
|        29 | 1993 | `		}else{` |
|       173 | 1994 | `			rc = iId > pOther->x.iVal ? 1 : (iId < pOther->x.iVal ? -1 : 0);` |
|         - | 1995 | `		}` |
|       229 | 1996 | `		return bRes1 ? rc : -rc;` |
|   2155192 | 1997 | `	}else if( VmIsUnorderedCmp(pObj1,pObj2) ){` |
|         - | 1998 | `		/* A NaN against a number or a string: php answers 1 in BOTH directions` |
|         - | 1999 | ``		 * (`NAN <=> 1` and `1 <=> NAN` are both 1), which is what leaves every`` |
|         - | 2000 | `		 * relational operator false at once. The rule lives HERE, not only in the` |
|         - | 2001 | `		 * operator arms, because everything else that orders values goes through` |
|         - | 2002 | ``		 * this comparator with no arm of its own: `in_array(NAN, ["NAN"])` was TRUE`` |
|         - | 2003 | ``		 * (php: false), `array_search` found it, and a `switch` matched it -- all`` |
|         - | 2004 | `		 * because the string branch below rendered the NaN as the bytes "NAN" and` |
|         - | 2005 | `		 * compared those. The precedence php gives null, bool, array and object is` |
|         - | 2006 | `		 * already spent above: VmIsUnorderedCmp screens those flags out, so` |
|         - | 2007 | ``		 * `NAN == true` stays the bool comparison it is there. */`` |
|       240 | 2008 | `		return 1;` |
|   2154954 | 2009 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 2010 | `		SyString s1,s2;` |
|   1295659 | 2011 | `		if( !bStrict ){` |
|         - | 2012 | `			/*` |
|         - | 2013 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 2014 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 2015 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 2016 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 2017 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 2018 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 2019 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 2020 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 2021 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 2022 | `			 * below, unchanged.` |
|         - | 2023 | `			 */` |
|    314878 | 2024 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 2025 | `				/*` |
|         - | 2026 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 2027 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 2028 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 2029 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 2030 | `				 * really does compare as doubles, so` |
|         - | 2031 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 2032 | `				 *` |
|         - | 2033 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 2034 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 2035 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 2036 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 2037 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 2038 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 2039 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 2040 | `				 *    one, no conversion involved -- which is why` |
|         - | 2041 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 2042 | `				 *    reach the same double.` |
|         - | 2043 | `				 *` |
|         - | 2044 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 2045 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 2046 | `				 */` |
|       429 | 2047 | `				int bBytes = 0;` |
|         - | 2048 | `				{` |
|       429 | 2049 | `					ph7_real r1 = 0, r2 = 0;` |
|       429 | 2050 | `					int iOf1 = 0, iOf2 = 0;` |
|       429 | 2051 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|       429 | 2052 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|       429 | 2053 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 2054 | `						bBytes = 1;` |
|       379 | 2055 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        36 | 2056 | `						return iOf1;` |
|       303 | 2057 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        19 | 2058 | `						return -iOf2;` |
|         - | 2059 | `					}` |
|         - | 2060 | `				}` |
|       385 | 2061 | `				if( !bBytes ){` |
|         - | 2062 | `					/* Perform a numeric comparison */` |
|       285 | 2063 | `					goto Numeric;` |
|         - | 2064 | `				}` |
|        50 | 2065 | `			}` |
|    157137 | 2066 | `		}` |
|         - | 2067 | `		/* Perform a strict string comparison.*/` |
|   1295333 | 2068 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        25 | 2069 | `			PH7_MemObjToString(pObj1);` |
|        12 | 2070 | `		}` |
|   1295333 | 2071 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        35 | 2072 | `			PH7_MemObjToString(pObj2);` |
|        17 | 2073 | `		}` |
|   1295333 | 2074 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   1295333 | 2075 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 2076 | `		/*` |
|         - | 2077 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 2078 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 2079 | `		 */` |
|   1295333 | 2080 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   1295333 | 2081 | `		if( rc == 0 ){` |
|    397882 | 2082 | `			if( s1.nByte != s2.nByte ){` |
|     24986 | 2083 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     12495 | 2084 | `			}` |
|    198958 | 2085 | `		}` |
|   1295333 | 2086 | `		return rc;` |
|    859300 | 2087 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|    428784 | 2088 | `Numeric:` |
|         - | 2089 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|    859582 | 2090 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       225 | 2091 | `			PH7_MemObjToNumeric(pObj1);` |
|       109 | 2092 | `		}` |
|    859582 | 2093 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       243 | 2094 | `			PH7_MemObjToNumeric(pObj2);` |
|       118 | 2095 | `		}` |
|    859582 | 2096 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 2097 | `			/*` |
|         - | 2098 | `			 * Symisc eXtension to the PHP language:` |
|         - | 2099 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 2100 | `			 */` |
|         - | 2101 | `			ph7_real r1,r2;` |
|         - | 2102 | `			/* Compare as reals */` |
|       488 | 2103 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        43 | 2104 | `				PH7_MemObjToReal(pObj1);` |
|        21 | 2105 | `			}` |
|       488 | 2106 | `			r1 = pObj1->rVal;` |
|       488 | 2107 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        27 | 2108 | `				PH7_MemObjToReal(pObj2);` |
|        13 | 2109 | `			}` |
|       488 | 2110 | `			r2 = pObj2->rVal;` |
|       488 | 2111 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 2112 | `				/*` |
|         - | 2113 | `				 * php's answer for an unordered pair, from either side: 1. The` |
|         - | 2114 | `				 * branch above catches every NaN that arrives AS a float; this one` |
|         - | 2115 | `				 * is for a NaN that only appears once both operands have been` |
|         - | 2116 | `				 * converted, and it must agree with it -- an antisymmetric answer` |
|         - | 2117 | ``				 * here (the old `NaN equals NaN, and is greater than everything`` |
|         - | 2118 | ``				 * else`) is what made `NAN === NAN` true and `1.5 > NAN` disagree`` |
|         - | 2119 | ``				 * with `NAN < 1.5`.`` |
|         - | 2120 | `				 */` |
|       ! 0 | 2121 | `				return 1;` |
|         - | 2122 | `			}` |
|       488 | 2123 | `			if( r1 > r2 ){` |
|        43 | 2124 | `				return 1;` |
|       448 | 2125 | `			}else if( r1 < r2 ){` |
|       220 | 2126 | `				return -1;` |
|         - | 2127 | `			}` |
|       231 | 2128 | `			return 0;` |
|       ! 0 | 2129 | `		}else{` |
|         - | 2130 | `			/* Integer comparison */` |
|    859098 | 2131 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|    274874 | 2132 | `				return 1;` |
|    584229 | 2133 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|    573677 | 2134 | `				return -1;` |
|         - | 2135 | `			}` |
|     10557 | 2136 | `			return 0;` |
|         - | 2137 | `		}` |
|         - | 2138 | `	}` |
|         - | 2139 | `	/* NOT REACHED */` |
|       ! 0 | 2140 | `	return 0;` |
|   1283269 | 2141 | `}` |
|         - | 2142 | `/*` |
|         - | 2143 | ` * Perform an addition operation of two ph7_values.` |
|         - | 2144 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 2145 | ` * is that the '+' operator is overloaded.` |
|         - | 2146 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 2147 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 2148 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 2149 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 2150 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 2151 | ` * be ignored.` |
|         - | 2152 | ` * This function take care of handling all the scenarios.` |
|         - | 2153 | ` */` |
|     27134 | 2154 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 2155 | `{` |
|     27139 | 2156 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2157 | `			/* Arithemtic operation */` |
|     22029 | 2158 | `			PH7_MemObjToNumeric(pObj1);` |
|     22029 | 2159 | `			PH7_MemObjToNumeric(pObj2);` |
|     22029 | 2160 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 2161 | `				/* Floating point arithmetic */` |
|         - | 2162 | `				ph7_real a,b;` |
|       129 | 2163 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        38 | 2164 | `					PH7_MemObjToReal(pObj1);` |
|        18 | 2165 | `				}` |
|       129 | 2166 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        57 | 2167 | `					PH7_MemObjToReal(pObj2);` |
|        28 | 2168 | `				}` |
|       129 | 2169 | `				a = pObj1->rVal;` |
|       129 | 2170 | `				b = pObj2->rVal;` |
|       129 | 2171 | `				pObj1->rVal = a+b;` |
|       129 | 2172 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2173 | `				/* Try to get an integer representation also */` |
|       129 | 2174 | `				MemObjTryIntger(&(*pObj1));` |
|        66 | 2175 | `			}else{` |
|         - | 2176 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 2177 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 2178 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 2179 | `				sxi64 a,b,r;` |
|     21903 | 2180 | `				a = pObj1->x.iVal;` |
|     21903 | 2181 | `				b = pObj2->x.iVal;` |
|     21903 | 2182 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 2183 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        11 | 2184 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        11 | 2185 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2186 | `#else` |
|         - | 2187 | `					pObj1->x.iVal = r;` |
|         - | 2188 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2189 | `#endif` |
|         6 | 2190 | `				}else{` |
|     21893 | 2191 | `					pObj1->x.iVal = r;` |
|     21893 | 2192 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2193 | `				}` |
|         - | 2194 | `			}` |
|     11017 | 2195 | `	}else{` |
|      5115 | 2196 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 2197 | `			ph7_hashmap *pMap;` |
|         - | 2198 | `			sxi32 rc;` |
|      5115 | 2199 | `			if( bAddStore ){` |
|         - | 2200 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 2201 | `				 */` |
|         3 | 2202 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2203 | `					/* Force a hashmap cast */` |
|       ! 0 | 2204 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 2205 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2206 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2207 | `						return rc;` |
|         - | 2208 | `					}` |
|       ! 0 | 2209 | `				}` |
|         - | 2210 | `				/* COW separate before in-place mutation */` |
|         3 | 2211 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|         2 | 2212 | `			}else{` |
|         - | 2213 | `				/* Create a new hashmap */` |
|      5113 | 2214 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      5113 | 2215 | `				if( pMap == 0){` |
|       ! 0 | 2216 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2217 | `					return SXERR_MEM;` |
|         - | 2218 | `				}` |
|         - | 2219 | `			}` |
|      5115 | 2220 | `			if( !bAddStore ){` |
|      5113 | 2221 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 2222 | `					/* Perform a hashmap duplication */` |
|      5113 | 2223 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      2559 | 2224 | `				}else{` |
|       ! 0 | 2225 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2226 | `						/* Simple insertion */` |
|       ! 0 | 2227 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 2228 | `					}` |
|         - | 2229 | `				}` |
|      2554 | 2230 | `			}` |
|         - | 2231 | `			/* Perform the union */` |
|      5115 | 2232 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      5115 | 2233 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      2560 | 2234 | `			}else{` |
|       ! 0 | 2235 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2236 | `					/* Simple insertion */` |
|       ! 0 | 2237 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 2238 | `				}` |
|         - | 2239 | `			}` |
|         - | 2240 | `			/* Reflect the change */` |
|      5115 | 2241 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 2242 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 2243 | `			}` |
|      5115 | 2244 | `			pObj1->x.pOther = pMap;` |
|      5115 | 2245 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      2555 | 2246 | `		}` |
|         - | 2247 | `	}` |
|     27139 | 2248 | `	return SXRET_OK;` |
|     13572 | 2249 | `}` |
|         - | 2250 | `/*` |
|         - | 2251 | ` * Return a printable representation of the type of a given` |
|         - | 2252 | ` * ph7_value.` |
|         - | 2253 | ` */` |
|         4 | 2254 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|         1 | 2255 | `{` |
|         5 | 2256 | `	const char *zType = "";` |
|         5 | 2257 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 2258 | `		zType = "null";` |
|         5 | 2259 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 2260 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 2261 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 2262 | `		zType = "double";` |
|         5 | 2263 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 2264 | `		zType = "int";` |
|         5 | 2265 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|         3 | 2266 | `		zType = "string";` |
|         4 | 2267 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2268 | `		zType = "bool";` |
|         3 | 2269 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 2270 | `		zType = "array";` |
|         1 | 2271 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 2272 | `		zType = "object";` |
|       ! 0 | 2273 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 2274 | `		zType = "resource";` |
|       ! 0 | 2275 | `	}` |
|         5 | 2276 | `	return zType;` |
|         1 | 2277 | `}` |
|         - | 2278 | `/*` |
|         - | 2279 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 2280 | ` * Store the dump in the given blob.` |
|         - | 2281 | ` */` |
|         - | 2282 | `/*` |
|         - | 2283 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2284 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2285 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2286 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2287 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2288 | ` */` |
|       170 | 2289 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         5 | 2290 | `{` |
|         - | 2291 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2292 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2293 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2294 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2295 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2296 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2297 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2298 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2299 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       175 | 2300 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2301 | `#else` |
|         - | 2302 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2303 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2304 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2305 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2306 | `	}else{` |
|         - | 2307 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2308 | `	}` |
|         - | 2309 | `#endif` |
|       175 | 2310 | `}` |
|         - | 2311 | `/*` |
|         - | 2312 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2313 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2314 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2315 | ` */` |
|       636 | 2316 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         5 | 2317 | `{` |
|         - | 2318 | `	/* print_r RENDERS through the string coercion -- unlike var_dump and` |
|         - | 2319 | `	 * var_export, which describe the value instead -- so php's NaN warning` |
|         - | 2320 | `	 * belongs here too, once per value it prints. */` |
|       636 | 2321 | `	if( (pObj->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       324 | 2322 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         3 | 2323 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 2324 | `			"unexpected NAN value was coerced to string");` |
|         1 | 2325 | `	}` |
|       641 | 2326 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|         7 | 2327 | `		return;` |
|         - | 2328 | `	}` |
|       635 | 2329 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2330 | `		if( pObj->x.iVal != 0 ){` |
|       ! 0 | 2331 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       ! 0 | 2332 | `		}` |
|       ! 0 | 2333 | `		return;` |
|         - | 2334 | `	}` |
|       635 | 2335 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2336 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2337 | `		 * non-strings into the output) */` |
|       417 | 2338 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       415 | 2339 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       205 | 2340 | `		}` |
|       417 | 2341 | `		return;` |
|         - | 2342 | `	}` |
|       222 | 2343 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       323 | 2344 | `}` |
|     11958 | 2345 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2346 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2347 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2348 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2349 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2350 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2351 | `	int nDepth,        /* Nesting level */` |
|         - | 2352 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2353 | `	)` |
|         5 | 2354 | `{` |
|     11963 | 2355 | `	sxi32 rc = SXRET_OK;` |
|         - | 2356 | `	int i;` |
|     11963 | 2357 | `	if( !ShowType ){` |
|         - | 2358 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2359 | `		 * containers render the Array/Object block (which the container` |
|         - | 2360 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       311 | 2361 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       243 | 2362 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2363 | `		}` |
|        72 | 2364 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|        68 | 2365 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2366 | `		}` |
|         5 | 2367 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         5 | 2368 | `		return SXRET_OK;` |
|         - | 2369 | `	}` |
|         - | 2370 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2371 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2372 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     20865 | 2373 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      9213 | 2374 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      4609 | 2375 | `	}` |
|     11657 | 2376 | `	if( isRef ){` |
|        65 | 2377 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        31 | 2378 | `	}` |
|     11657 | 2379 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       211 | 2380 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       211 | 2381 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2382 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|         7 | 2383 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|         7 | 2384 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|         7 | 2385 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|         7 | 2386 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|         3 | 2387 | `			}` |
|         7 | 2388 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         7 | 2389 | `			return SXRET_OK;` |
|         - | 2390 | `		}` |
|       205 | 2391 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       205 | 2392 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       205 | 2393 | `		return rc;` |
|         - | 2394 | `	}` |
|     11451 | 2395 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       519 | 2396 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|       519 | 2397 | `		return SXRET_OK;` |
|         - | 2398 | `	}` |
|     10937 | 2399 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      1085 | 2400 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|      1085 | 2401 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1085 | 2402 | `		return rc;` |
|         - | 2403 | `	}` |
|      9857 | 2404 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      3143 | 2405 | `		if( pObj->x.iVal != 0 ){` |
|      1869 | 2406 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|       937 | 2407 | `		}else{` |
|      1279 | 2408 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2409 | `		}` |
|      3143 | 2410 | `		return SXRET_OK;` |
|         - | 2411 | `	}` |
|      6719 | 2412 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2413 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2414 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       175 | 2415 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       175 | 2416 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       175 | 2417 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       175 | 2418 | `		return SXRET_OK;` |
|         - | 2419 | `	}` |
|      6549 | 2420 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      2839 | 2421 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      2839 | 2422 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      2839 | 2423 | `		return SXRET_OK;` |
|         - | 2424 | `	}` |
|      3715 | 2425 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      3715 | 2426 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      3715 | 2427 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      3443 | 2428 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      1719 | 2429 | `		}` |
|      3715 | 2430 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      3715 | 2431 | `		return SXRET_OK;` |
|         - | 2432 | `	}` |
|       ! 0 | 2433 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2434 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2435 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2436 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2437 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2438 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2439 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2440 | `		return SXRET_OK;` |
|         - | 2441 | `	}` |
|         - | 2442 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2443 | `	{` |
|       ! 0 | 2444 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2445 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2446 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2447 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2448 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2449 | `	}` |
|       ! 0 | 2450 | `	return rc;` |
|      5984 | 2451 | `}` |
|         - | 2452 |  |
