# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1149/1294 lines (88.79%)

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
|         - |   12 | `#if defined(PHL_VALUE_CENSUS)` |
|         - |   13 | `/*` |
|         - |   14 | ` * PHL_VALUE_CENSUS -- which CALL SITE spends the engine's value primitives.` |
|         - |   15 | ` * ---------------------------------------------------------------------------` |
|         - |   16 | ` * Compiled out entirely unless PHL_VALUE_CENSUS is defined; see` |
|         - |   17 | ` * build-aux/valuecensus.sh, which builds it and resolves what it prints.` |
|         - |   18 | ` *` |
|         - |   19 | ` * The fourth instrument, and it exists because the other three cannot see this.` |
|         - |   20 | ` * The heap census answers "where are the bytes"; the lookup census answers "which` |
|         - |   21 | ` * line hashes the names". PH7_MemObjRelease allocates nothing and looks nothing` |
|         - |   22 | ` * up, so it appears in NEITHER -- and it is the most-called function in the` |
|         - |   23 | ` * engine: 2.72 billion calls on the ecosystem gate's phpcs step, 43.1% of them on` |
|         - |   24 | ` * a value that owned nothing. A whole-program counter said that` |
|         - |   25 | ` * much; it could not say WHICH of the ~700 call sites made those calls, which is` |
|         - |   26 | ` * the question a design has to answer.` |
|         - |   27 | ` *` |
|         - |   28 | ` * One record per (return address, primitive), so a site is a place in the SOURCE` |
|         - |   29 | ` * and not a function: the release inside VmOperandStackRecycle's loop and the one` |
|         - |   30 | ` * in VmPopOperand are two rows, which is what a change is aimed at. Addresses are` |
|         - |   31 | ` * emitted relative to the PIE load base (the ADDRESS of __executable_start is that` |
|         - |   32 | ` * base at run time), so addr2line takes them exactly as printed -- the other two` |
|         - |   33 | ` * censuses' convention, for the same reason.` |
|         - |   34 | ` *` |
|         - |   35 | `` * `nWork` is the half the call-count alone cannot give: how many of a site's calls`` |
|         - |   36 | ` * had anything to DO. For a release that is the slow path (it owned a string, a` |
|         - |   37 | ` * container reference or an AUX carrier); for a load/store it is a container` |
|         - |   38 | ` * reference taken; for an init it is always 1. A site with a large count and a` |
|         - |   39 | ` * near-zero nWork is the engine building and tearing down slots that never held` |
|         - |   40 | ` * anything -- which is P10 item 1, and it is what this instrument was built to` |
|         - |   41 | ` * find.` |
|         - |   42 | ` *` |
|         - |   43 | ` * The table is fixed-size and static, for the other censuses' reason: it must not` |
|         - |   44 | ` * allocate through the allocator whose values it is counting. Nothing is ever` |
|         - |   45 | ` * deleted (a site is a code address), so a full table refuses to record and says` |
|         - |   46 | ` * TRUNCATED rather than under-count.` |
|         - |   47 | ` */` |
|         - |   48 | `#include <stdio.h>` |
|         - |   49 | `#include <stdlib.h>` |
|         - |   50 |  |
|         - |   51 | `extern char __executable_start[];   /* its ADDRESS is the PIE load base */` |
|         - |   52 |  |
|         - |   53 | `typedef struct phl_vcensus_rec phl_vcensus_rec;` |
|         - |   54 | `struct phl_vcensus_rec {` |
|         - |   55 | `	void *pSite;     /* PHL_VCENSUS_SITE() at the door; 0 = free slot */` |
|         - |   56 | `	sxu32 iKind;     /* PHL_VC_* -- which primitive was called */` |
|         - |   57 | `	sxu64 nCall;     /* calls made from here */` |
|         - |   58 | `	sxu64 nWork;     /* how many of them had anything to do */` |
|         - |   59 | `};` |
|         - |   60 | `#define PHL_VCENSUS_SLOTS 8192` |
|         - |   61 | `static struct {` |
|         - |   62 | `	int bReady;      /* 0 = untouched, 1 = live */` |
|         - |   63 | `	int bFull;       /* the table filled; recording stopped */` |
|         - |   64 | `	phl_vcensus_rec aRec[PHL_VCENSUS_SLOTS];` |
|         - |   65 | `	sxu64 aCall[PHL_VC_KINDS],aWork[PHL_VC_KINDS];` |
|         - |   66 | `} sVCensus;` |
|         - |   67 |  |
|         - |   68 | `static void VCensusDump(void)` |
|         - |   69 | `{` |
|         - |   70 | `	const char *zOut = getenv("PHL_VCENSUS_OUT");` |
|         - |   71 | `	FILE *pOut = zOut ? fopen(zOut,"w") : stderr;` |
|         - |   72 | `	sxu32 i;` |
|         - |   73 | `	if( pOut == 0 ){` |
|         - |   74 | `		pOut = stderr;` |
|         - |   75 | `	}` |
|         - |   76 | `	for( i = 0 ; i < PHL_VC_KINDS ; ++i ){` |
|         - |   77 | `		fprintf(pOut,"# kind %u %llu %llu%s\n",i,` |
|         - |   78 | `			(unsigned long long)sVCensus.aCall[i],(unsigned long long)sVCensus.aWork[i],` |
|         - |   79 | `			sVCensus.bFull ? "  TRUNCATED" : "");` |
|         - |   80 | `	}` |
|         - |   81 | `	for( i = 0 ; i < PHL_VCENSUS_SLOTS ; ++i ){` |
|         - |   82 | `		phl_vcensus_rec *pRec = &sVCensus.aRec[i];` |
|         - |   83 | `		if( pRec->pSite == 0 ){` |
|         - |   84 | `			continue;` |
|         - |   85 | `		}` |
|         - |   86 | `		fprintf(pOut,"SITE 0x%lx %u %llu %llu\n",` |
|         - |   87 | `			(unsigned long)((char *)pRec->pSite - __executable_start),` |
|         - |   88 | `			pRec->iKind,` |
|         - |   89 | `			(unsigned long long)pRec->nCall,(unsigned long long)pRec->nWork);` |
|         - |   90 | `	}` |
|         - |   91 | `	if( pOut != stderr ){` |
|         - |   92 | `		fclose(pOut);` |
|         - |   93 | `	}` |
|         - |   94 | `}` |
|         - |   95 | `PH7_PRIVATE void PH7_ValueCensusNote(void *pSite,sxu32 iKind,int bWork)` |
|         - |   96 | `{` |
|         - |   97 | `	/* Fibonacci scramble: the low bits of a code address are not a key, and the` |
|         - |   98 | `	 * kind has to be in it or one line's release and load share a slot. */` |
|         - |   99 | `	sxu64 x = (sxu64)(sxuptr)pSite ^ ((sxu64)iKind * (sxu64)0x9e3779b97f4a7c15ULL);` |
|         - |  100 | `	sxu32 i,n;` |
|         - |  101 | `	if( !sVCensus.bReady ){` |
|         - |  102 | `		sVCensus.bReady = 1;` |
|         - |  103 | `		atexit(VCensusDump);` |
|         - |  104 | `	}` |
|         - |  105 | `	x ^= x >> 33; x *= (sxu64)0xff51afd7ed558ccdULL; x ^= x >> 29;` |
|         - |  106 | `	i = (sxu32)x & (PHL_VCENSUS_SLOTS - 1);` |
|         - |  107 | `	for( n = 0 ; n < PHL_VCENSUS_SLOTS ; ++n ){` |
|         - |  108 | `		phl_vcensus_rec *pRec = &sVCensus.aRec[i];` |
|         - |  109 | `		if( pRec->pSite == 0 ){` |
|         - |  110 | `			pRec->pSite = pSite;` |
|         - |  111 | `			pRec->iKind = iKind;` |
|         - |  112 | `		}` |
|         - |  113 | `		if( pRec->pSite == pSite && pRec->iKind == iKind ){` |
|         - |  114 | `			pRec->nCall++;` |
|         - |  115 | `			pRec->nWork += bWork ? 1 : 0;` |
|         - |  116 | `			sVCensus.aCall[iKind]++;` |
|         - |  117 | `			sVCensus.aWork[iKind] += bWork ? 1 : 0;` |
|         - |  118 | `			return;` |
|         - |  119 | `		}` |
|         - |  120 | `		i = (i + 1) & (PHL_VCENSUS_SLOTS - 1);` |
|         - |  121 | `	}` |
|         - |  122 | `	sVCensus.bFull = 1;   /* said out loud in the dump rather than counted wrong */` |
|         - |  123 | `}` |
|         - |  124 | `#endif /* PHL_VALUE_CENSUS */` |
|         - |  125 |  |
|         - |  126 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|         - |  127 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|         - |  128 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|         - |  129 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|         - |  130 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|         - |  131 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|         - |  132 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|         - |  133 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|         - |  134 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|         - |  135 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |  136 | `{` |
|         5 |  137 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|         - |  138 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|         5 |  139 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|         5 |  140 | `}` |
|         - |  141 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |  142 | `{` |
|         5 |  143 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|         - |  144 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|         - |  145 | `	 * from the minuend's. */` |
|         5 |  146 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|         5 |  147 | `}` |
|         - |  148 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|         5 |  149 | `{` |
|         5 |  150 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|         5 |  151 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|         5 |  152 | `		return 0;` |
|         - |  153 | `	}` |
|         5 |  154 | `	if( a == -1 ){` |
|         1 |  155 | `		return b == SMALLEST_INT64;` |
|         - |  156 | `	}` |
|         5 |  157 | `	if( b == -1 ){` |
|         1 |  158 | `		return a == SMALLEST_INT64;` |
|         - |  159 | `	}` |
|         5 |  160 | `	if( a > 0 ){` |
|         5 |  161 | `		if( b > 0 ){` |
|         5 |  162 | `			return a > LARGEST_INT64 / b;` |
|       ! 0 |  163 | `		}else{` |
|         1 |  164 | `			return b < SMALLEST_INT64 / a;` |
|         - |  165 | `		}` |
|       ! 0 |  166 | `	}else{` |
|         1 |  167 | `		if( b > 0 ){` |
|         1 |  168 | `			return a < SMALLEST_INT64 / b;` |
|       ! 0 |  169 | `		}else{` |
|         1 |  170 | `			return b < LARGEST_INT64 / a;` |
|         - |  171 | `		}` |
|         - |  172 | `	}` |
|         5 |  173 | `}` |
|         - |  174 | `#endif` |
|         - |  175 |  |
|         - |  176 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|         - |  177 | ` * by any subsystem that works with ph7_value.` |
|         - |  178 | ` */` |
|      8716 |  179 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|         5 |  180 | `{` |
|      8721 |  181 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|      8230 |  182 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|         - |  183 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|         - |  184 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|         - |  185 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|         - |  186 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|         - |  187 | `	 * A value that IS a float is a float whatever it has cached. */` |
|      7672 |  188 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|      7523 |  189 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|      5273 |  190 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|      1644 |  191 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|        46 |  192 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|        46 |  193 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|       ! 0 |  194 | `	return "unknown";` |
|      4359 |  195 | `}` |
|         - |  196 |  |
|         - |  197 | `/*` |
|         - |  198 | ` * Notes on memory objects [i.e: ph7_value].` |
|         - |  199 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|         - |  200 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|         - |  201 | ` * Each ph7_values struct may cache multiple representations (string,` |
|         - |  202 | ` * integer etc.) of the same value.` |
|         - |  203 | ` */` |
|         - |  204 | `/*` |
|         - |  205 | ` * TRUE when a double is what an int64 can hold exactly -- php's` |
|         - |  206 | ` * ZEND_DOUBLE_FITS_LONG with its non-finite screen folded in. The bounds are` |
|         - |  207 | ` * tested in DOUBLE space and the arithmetic there is exact: -2^63 is a double` |
|         - |  208 | ` * to the bit and so is +2^63, one past the range, with no double in between it` |
|         - |  209 | `` * and LARGEST_INT64. Hence `>=` on the way down and `<` on the way up. NaN and`` |
|         - |  210 | ` * both infinities fail one of the two comparisons, so no libm predicate is` |
|         - |  211 | ` * needed to screen them.` |
|         - |  212 | ` */` |
|     30936 |  213 | `PH7_PRIVATE int PH7_RealFitsInt64(double r)` |
|         5 |  214 | `{` |
|     30941 |  215 | `	return r >= -9223372036854775808.0 && r < 9223372036854775808.0;` |
|         5 |  216 | `}` |
|         - |  217 | `/*` |
|         - |  218 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer -- php's` |
|         - |  219 | ` * zend_dval_to_lval, the answer every CAST site gives for a double no int can` |
|         - |  220 | ` * hold: NaN and both infinities are 0, and a finite out-of-range value WRAPS` |
|         - |  221 | `` * modulo 2^64 into the signed band (`(int)1e19` is -8446744073709551616,`` |
|         - |  222 | `` * `(int)1e30` is 5076964154930102272, `(int)1e100` is 0 because every one of`` |
|         - |  223 | ` * its low 64 bits is).` |
|         - |  224 | ` *` |
|         - |  225 | ` * PHL used to answer PHP_INT_MIN for all of them, in silence -- a recorded` |
|         - |  226 | ` * divergence, and a silent wrong answer wherever a program casts a computed` |
|         - |  227 | ` * float. The warning php prints beside the value is the cast SITE's to raise:` |
|         - |  228 | ` * this is also the conversion an int representation is speculatively cached` |
|         - |  229 | ` * through (MemObjTryIntger), where php says nothing at all.` |
|         - |  230 | ` *` |
|         - |  231 | ` * php reaches the wrap through fmod(d, 2^64); the same answer comes out of the` |
|         - |  232 | ` * IEEE bits with no libm. A double of magnitude >= 2^63 is already an exact` |
|         - |  233 | ` * integer -- its mantissa is scaled by 2^11 at least -- so the low 64 bits are` |
|         - |  234 | ` * the 53-bit mantissa shifted LEFT, which is 0 once the shift reaches 64.` |
|         - |  235 | ` */` |
|     29768 |  236 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r)` |
|         5 |  237 | `{` |
|         - |  238 | `  union { double d; sxu64 u; } bits;` |
|         - |  239 | `  sxu64 uMag;` |
|         - |  240 | `  int iShift;` |
|     29773 |  241 | `  if( PH7_RealFitsInt64(r) ){` |
|         - |  242 | `    /* In range: php truncates toward zero, and so does C. */` |
|     28017 |  243 | `    return (sxi64)r;` |
|         - |  244 | `  }` |
|      1761 |  245 | `  if( PH7_IS_NAN(r) \|\| PH7_IS_INF(r) ){` |
|       673 |  246 | `    return 0;` |
|         - |  247 | `  }` |
|      1093 |  248 | `  bits.d = r;` |
|         - |  249 | `  /* Unbiased exponent, minus the 52 fraction bits: the power of two the` |
|         - |  250 | `  ** mantissa is scaled by. \|r\| >= 2^63 puts it at 11 or more. */` |
|      1093 |  251 | `  iShift = (int)((bits.u >> 52) & 0x7FF) - 1023 - 52;` |
|      1093 |  252 | `  if( iShift >= 64 ){` |
|         - |  253 | `    /* Every set bit sits above the 64th, so the residue is 0 -- and the shift` |
|         - |  254 | `    ** below would be undefined. */` |
|       203 |  255 | `    return 0;` |
|         - |  256 | `  }` |
|       895 |  257 | `  uMag = ((bits.u & 0x000FFFFFFFFFFFFFULL) \| 0x0010000000000000ULL) << iShift;` |
|       895 |  258 | `  if( bits.u >> 63 ){` |
|         - |  259 | ``     /* Unsigned negation is the two's-complement residue php's `dmod += 2^64` `` |
|         - |  260 | `    ** arrives at, and is defined for every input including 0. */` |
|        51 |  261 | `    uMag = (sxu64)0 - uMag;` |
|        25 |  262 | `  }` |
|       895 |  263 | `  return (sxi64)uMag;` |
|     14867 |  264 | `}` |
|     29260 |  265 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|         5 |  266 | `{` |
|         - |  267 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  268 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|         - |  269 | `	 * is omitted from the build.` |
|         - |  270 | `	 */` |
|         - |  271 | `	return pObj->rVal;` |
|         - |  272 | `#else` |
|     29265 |  273 | `	return PH7_RealToInt64(pObj->rVal);` |
|         - |  274 | `#endif` |
|         5 |  275 | `}` |
|         - |  276 | `/*` |
|         - |  277 | `` * php's `Warning: The float %s is not representable as an int, cast occurred`,`` |
|         - |  278 | ` * printed BESIDE the wrapped value MemObjRealToInt answers -- at every CAST` |
|         - |  279 | `` * site, which is what php's zend_dval_to_lval raises it from: `(int)$f`,`` |
|         - |  280 | `` * `intval()`, `settype()`, the printf integer conversions, and a native`` |
|         - |  281 | ` * subscript that reads an int out of its offset.` |
|         - |  282 | ` *` |
|         - |  283 | `` * Not a DEPRECATION: php's other float->int diagnostic (`Implicit conversion`` |
|         - |  284 | `` * from float %s to int loses precision`) is the E_DEPRECATED that the scope policy refuses`` |
|         - |  285 | ` * outright with a TypeError, and it fires at the sites this one does NOT --` |
|         - |  286 | ` * the operators, the array key, the int parameter, none of which reach a cast` |
|         - |  287 | ` * here because the refusal comes first. An explicit cast is never lossy in` |
|         - |  288 | ` * php's eyes, so this warning is all it says. The two other conversions that` |
|         - |  289 | ` * read an int out of a float say nothing at all and must not call this: the` |
|         - |  290 | ` * speculative int representation (MemObjTryIntger) and php's string-offset` |
|         - |  291 | ` * cast, which has a message of its own.` |
|         - |  292 | ` *` |
|         - |  293 | `` * The value is rendered the way php's `%.*H` renders it -- the shortest`` |
|         - |  294 | ` * decimal that round-trips, the shape var_dump and serialize already share.` |
|         - |  295 | ` */` |
|       832 |  296 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r)` |
|         4 |  297 | `{` |
|         - |  298 | `	SyBlob sVal;` |
|         - |  299 | `	char zVal[64];` |
|       836 |  300 | `	if( pVm == 0 \|\| PH7_RealFitsInt64(r) ){` |
|       618 |  301 | `		return;` |
|         - |  302 | `	}` |
|       221 |  303 | `	SyBlobInitFromBuf(&sVal,zVal,(sxu32)sizeof(zVal) - 1);` |
|       221 |  304 | `	PH7_AppendShortestReal(&sVal,r);` |
|       221 |  305 | `	zVal[SyBlobLength(&sVal)] = 0;   /* the blob is LOCKED: it truncates, never grows */` |
|       330 |  306 | `	VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       109 |  307 | `		"The float %s is not representable as an int, cast occurred",zVal);` |
|       420 |  308 | `}` |
|         - |  309 | `/* The same warning asked of a VALUE: only a float can carry one, and the flag` |
|         - |  310 | ` * test mirrors the conversion's own routing (MemObjIntValue reads MEMOBJ_REAL` |
|         - |  311 | ` * first), so the diagnostic and the answer always describe the same branch. */` |
|   1185858 |  312 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj)` |
|         5 |  313 | `{` |
|   1185863 |  314 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|   1185521 |  315 | `		return;` |
|         - |  316 | `	}` |
|       345 |  317 | `	PH7_RealWarnIntCast(pObj->pVm,(double)pObj->rVal);` |
|    592877 |  318 | `}` |
|         - |  319 | `/*` |
|         - |  320 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|         - |  321 | ` * to a 64-bit integer.` |
|         - |  322 | ` */` |
|    985716 |  323 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|         5 |  324 | `{` |
|    985721 |  325 | `	sxi64 iVal = 0;` |
|    985721 |  326 | `	if( pVal->nByte <= 0 ){` |
|       ! 0 |  327 | `		return 0;` |
|         - |  328 | `	}` |
|    985721 |  329 | `	if( pVal->zString[0] == '0' ){` |
|         - |  330 | `		sxi32 c;` |
|    356862 |  331 | `		if( pVal->nByte == sizeof(char) ){` |
|    347171 |  332 | `			return 0;` |
|         - |  333 | `		}` |
|      9696 |  334 | `		c = pVal->zString[1];` |
|      9696 |  335 | `		if( c  == 'x' \|\| c == 'X' ){` |
|         - |  336 | `			/* Hex digit stream */` |
|       740 |  337 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      9328 |  338 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|         - |  339 | `			/* Binary digit stream */` |
|       285 |  340 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|      8819 |  341 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|         - |  342 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|         - |  343 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|        21 |  344 | `			if( pVal->nByte > 2 ){` |
|        21 |  345 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|        10 |  346 | `			}` |
|        11 |  347 | `		}else{` |
|         - |  348 | `			/* Legacy octal digit stream (leading 0) */` |
|      8657 |  349 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  350 | `		}` |
|      4843 |  351 | `	}else{` |
|         - |  352 | `		/* Decimal digit stream */` |
|    628864 |  353 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|         - |  354 | `	}` |
|    638555 |  355 | `	return iVal;` |
|    492191 |  356 | `}` |
|         - |  357 | `/*` |
|         - |  358 | ` * TRUE when the numeric PREFIX that ends at zTail is float-SHAPED -- it carries` |
|         - |  359 | ` * a '.' or a complete exponent. This is php's is_numeric_string answering` |
|         - |  360 | ` * IS_DOUBLE, and it decides which of two entirely different readings the bytes` |
|         - |  361 | ` * get: an integer-shaped run is read from its DIGITS, a float-shaped one from` |
|         - |  362 | ` * the double they spell.` |
|         - |  363 | ` */` |
|   1180131 |  364 | `static int MemObjNumericPrefixIsFloat(ph7_value *pObj,const char *zTail)` |
|         5 |  365 | `{` |
|   1180136 |  366 | `	const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|   3935555 |  367 | `	while( z < zTail ){` |
|   2755710 |  368 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       290 |  369 | `			return TRUE;` |
|         - |  370 | `		}` |
|   2755424 |  371 | `		z++;` |
|         5 |  372 | `	}` |
|   1179850 |  373 | `	return FALSE;` |
|    590058 |  374 | `}` |
|         - |  375 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  376 | `/*` |
|         - |  377 | ` * php's zend_dval_to_lval_cap: the double->int conversion a NUMERIC STRING` |
|         - |  378 | ` * takes, which is not the one a real float takes. This one SATURATES at the` |
|         - |  379 | ` * int64 bounds and answers 0 for a value that is not finite, where the cast of` |
|         - |  380 | ` * an actual float answers PHP_INT_MIN for every out-of-range case` |
|         - |  381 | ` * (MemObjRealToInt -- a recorded divergence). PHL has always` |
|         - |  382 | `` * saturated the integer-shaped overflow, so `(int)"99999999999999999999"` is`` |
|         - |  383 | ` * PHP_INT_MAX in both engines; this is the same rule for the shapes that reach` |
|         - |  384 | ` * it through a double.` |
|         - |  385 | ` */` |
|       210 |  386 | `static sxi64 MemObjRealToIntCap(ph7_real r)` |
|         3 |  387 | `{` |
|         - |  388 | `	/* NaN fails both comparisons and either infinity fails one of them, so this` |
|         - |  389 | `	 * screens all three without a libm predicate. */` |
|       213 |  390 | `	if( !(r >= -1.7976931348623157e308 && r <= 1.7976931348623157e308) ){` |
|        16 |  391 | `		return 0;` |
|         - |  392 | `	}` |
|       199 |  393 | `	if( r >= 9223372036854775808.0 ){    /* +2^63, exact in double space */` |
|        24 |  394 | `		return LARGEST_INT64;` |
|         - |  395 | `	}` |
|       176 |  396 | `	if( r < -9223372036854775808.0 ){` |
|         5 |  397 | `		return SMALLEST_INT64;` |
|         - |  398 | `	}` |
|       172 |  399 | `	return (sxi64)r;` |
|       108 |  400 | `}` |
|         - |  401 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  402 | `/*` |
|         - |  403 | ` * Return some kind of 64-bit integer value which is the best we can` |
|         - |  404 | ` * do at representing the value that pObj describes as a string` |
|         - |  405 | ` * representation.` |
|         - |  406 | ` */` |
|   1179413 |  407 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|         5 |  408 | `{` |
|   1179418 |  409 | `	sxi64 iVal = 0;` |
|         - |  410 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   1179418 |  411 | `	const char *zTail = 0;` |
|   1179413 |  412 | `	if( PH7_MemObjStringNumericPrefix(pObj,&zTail)` |
|   1179407 |  413 | `	 && MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|         - |  414 | `		/* A float-shaped string is a DOUBLE first and an int second, which is the` |
|         - |  415 | ``		 * only reading that makes `(int)"1e3"` the 1000 it says: reading its`` |
|         - |  416 | `		 * digits stops at the 'e' and answers the mantissa's integer part, so` |
|         - |  417 | `		 * "1e3" was 1, "1.5e2" was 1 and "-2e2" was -2. The '.' forms were wrong` |
|         - |  418 | `		 * the same way wherever the double rounds away from the digits --` |
|         - |  419 | ``		 * `(int)"0.9999999999999999999"` is 1, not 0. */`` |
|       213 |  420 | `		ph7_real rVal = 0.0;` |
|       213 |  421 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),` |
|         - |  422 | `			(void *)&rVal,0);` |
|       213 |  423 | `		if( pOverflow ){` |
|         - |  424 | `			/* php reports no overflow for a float-shaped string however large it` |
|         - |  425 | `			 * is: it was always going to be a double, so no digits were lost. */` |
|       ! 0 |  426 | `			*pOverflow = 0;` |
|       ! 0 |  427 | `		}` |
|       213 |  428 | `		return MemObjRealToIntCap(rVal);` |
|         - |  429 | `	}` |
|         - |  430 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  431 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|         - |  432 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|         - |  433 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|   1179208 |  434 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|   1179208 |  435 | `	return iVal;` |
|    589703 |  436 | `}` |
|         - |  437 | `/*` |
|         - |  438 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|         - |  439 | ` * Return SXRET_OK if the magic method is available and have been` |
|         - |  440 | ` * successfully called. Any other return value indicates failure.` |
|         - |  441 | ` */` |
|      3379 |  442 | `static sxi32 MemObjCallClassCastMethod(` |
|         - |  443 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|         - |  444 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|         - |  445 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|         - |  446 | `	sxu32 nLen,                /* Method name length */` |
|         - |  447 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|         - |  448 | `	)` |
|         5 |  449 | `{` |
|         - |  450 | `	ph7_class_method *pMethod;` |
|         - |  451 | `	/* Check if the method is available */` |
|      3384 |  452 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|      3384 |  453 | `	if( pMethod == 0 ){` |
|         - |  454 | `		/* No such method */` |
|         5 |  455 | `		return SXERR_NOTFOUND;` |
|         - |  456 | `	}` |
|         - |  457 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|         - |  458 | `	 * that threw must not be reported as a successful call, or the caller` |
|         - |  459 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|         - |  460 | `	 * printed "Object" after a caught __toString() throw). */` |
|         - |  461 | `	{` |
|      3380 |  462 | `		SyString *pSavedNative = PH7_VmImplicitCallerArm(&(*pVm));` |
|      3380 |  463 | `		sxi32 rc = PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|      3380 |  464 | `		pVm->pNativeFrameName = pSavedNative;` |
|      3380 |  465 | `		return rc;` |
|         - |  466 | `	}` |
|      1693 |  467 | `}` |
|         - |  468 | `/*` |
|         - |  469 | ` * The number an object's own STRING is -- php's cast_object with IS_LONG /` |
|         - |  470 | ` * IS_DOUBLE for the one class that answers those (PH7_CLASS_NUM_AS_STRING).` |
|         - |  471 | ` *` |
|         - |  472 | ` * Both casts read the same text, so this answers the integer and, when the` |
|         - |  473 | `` * caller wants it, writes the float beside it: `(int)$x` on `<c>2.5</c>` is 2`` |
|         - |  474 | `` * and `(float)$x` is 2.5, exactly as the two string conversions would give.`` |
|         - |  475 | ` * Silent: this class HAS an answer, so php raises nothing.` |
|         - |  476 | ` */` |
|         8 |  477 | `static sxi64 MemObjIntFromClassString(ph7_vm *pVm,ph7_class_instance *pThis,ph7_real *pReal)` |
|         1 |  478 | `{` |
|         - |  479 | `	ph7_value sText;` |
|         9 |  480 | `	sxi64 iVal = 0;` |
|         9 |  481 | `	if( pReal ){` |
|         3 |  482 | `		*pReal = 0;` |
|         1 |  483 | `	}` |
|         9 |  484 | `	if( pVm == 0 \|\| pThis == 0 ){` |
|       ! 0 |  485 | `		return 0;` |
|         - |  486 | `	}` |
|         9 |  487 | `	PH7_MemObjInit(pVm,&sText);` |
|         8 |  488 | `	if( MemObjCallClassCastMethod(pVm,pThis,"__toString",sizeof("__toString")-1,&sText)` |
|         9 |  489 | `		== SXRET_OK && (sText.iFlags & MEMOBJ_STRING) ){` |
|         9 |  490 | `		iVal = MemObjStringToInt(&sText,0);` |
|         9 |  491 | `		if( pReal ){` |
|         4 |  492 | `			SyStrToReal((const char *)SyBlobData(&sText.sBlob),` |
|         1 |  493 | `				SyBlobLength(&sText.sBlob),(void *)pReal,0);` |
|         1 |  494 | `		}` |
|         4 |  495 | `	}` |
|         9 |  496 | `	PH7_MemObjRelease(&sText);` |
|         9 |  497 | `	return iVal;` |
|         5 |  498 | `}` |
|         - |  499 | `/*` |
|         - |  500 | ` * Return some kind of integer value which is the best we can` |
|         - |  501 | ` * do at representing the value that pObj describes as an integer.` |
|         - |  502 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|         - |  503 | ` * a floating-point then  the value returned is the integer part.` |
|         - |  504 | ` * If pObj is a string, then we make an attempt to convert it into` |
|         - |  505 | ` * a integer and return that.` |
|         - |  506 | ` * If pObj represents a NULL value, return 0.` |
|         - |  507 | ` */` |
|   1181315 |  508 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|         5 |  509 | `{` |
|         - |  510 | `	sxi32 iFlags;` |
|   1181320 |  511 | `	iFlags = pObj->iFlags;` |
|   1181320 |  512 | `	if (iFlags & MEMOBJ_REAL ){` |
|       370 |  513 | `		return MemObjRealToInt(&(*pObj));` |
|   1180954 |  514 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      3403 |  515 | `		return pObj->x.iVal;` |
|   1177556 |  516 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  517 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|         - |  518 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|         - |  519 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|   1176868 |  520 | `		return MemObjStringToInt(&(*pObj),0);` |
|       693 |  521 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|       589 |  522 | `		return 0;` |
|       107 |  523 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  524 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|         - |  525 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|         - |  526 | `		 * followed php; int/float did not.) */` |
|        36 |  527 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        36 |  528 | `		sxu32 n = pMap->nEntry;` |
|        36 |  529 | `		PH7_HashmapUnref(pMap);` |
|        36 |  530 | `		return n > 0 ? 1 : 0;` |
|        73 |  531 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  532 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|         - |  533 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|         - |  534 | ``		 * (the scope policy), so `(int)$obj` silently returned user data where php diagnoses.`` |
|         - |  535 | `		 *` |
|         - |  536 | `		 * Two classes are php's own exception -- the curl easy and multi handles,` |
|         - |  537 | `		 * which answer their OBJECT HANDLE and say nothing, because they used to be` |
|         - |  538 | ``		 * resources and `(int)$h` used to be the resource id (PH7_CLASS_HANDLE_ID).`` |
|         - |  539 | `		 * Every door that asks an int of a value comes through here, which is what` |
|         - |  540 | ``		 * makes intval(), settype(), `%d` and array_sum() agree with the cast. */`` |
|        46 |  541 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        46 |  542 | `		if( pInst && pInst->pClass && PH7_ClassCastsToHandleId(pInst->pClass) ){` |
|        21 |  543 | `			sxi64 iId = (sxi64)pInst->nObjId;` |
|        21 |  544 | `			PH7_ClassInstanceUnref(pInst);` |
|        21 |  545 | `			return iId;` |
|         - |  546 | `		}` |
|        26 |  547 | `		if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|         - |  548 | `			/* php's SimpleXMLElement casts to the NUMBER ITS TEXT IS, silently. */` |
|         7 |  549 | `			sxi64 iVal = MemObjIntFromClassString(pObj->pVm,pInst,0);` |
|         7 |  550 | `			PH7_ClassInstanceUnref(pInst);` |
|         7 |  551 | `			return iVal;` |
|         - |  552 | `		}` |
|        19 |  553 | `		if( pInst && pInst->pClass ){` |
|        27 |  554 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        16 |  555 | `				"Object of class %z could not be converted to int",&pInst->pClass->sDisp);` |
|         8 |  556 | `		}` |
|        19 |  557 | `		PH7_ClassInstanceUnref(pInst);` |
|        19 |  558 | `		return 1;` |
|        29 |  559 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         - |  560 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|         - |  561 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|        29 |  562 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  563 | `	}` |
|         - |  564 | `	/* CANT HAPPEN */` |
|       ! 0 |  565 | `	return 0;` |
|    590645 |  566 | `}` |
|         - |  567 | `/*` |
|         - |  568 | ` * Return some kind of real value which is the best we can` |
|         - |  569 | ` * do at representing the value that pObj describes as a real.` |
|         - |  570 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|         - |  571 | ` * integer then the integer  is promoted to real and that value` |
|         - |  572 | ` * is returned.` |
|         - |  573 | ` * If pObj is a string, then we make an attempt to convert it` |
|         - |  574 | ` * into a real and return that.` |
|         - |  575 | ` * If pObj represents a NULL value, return 0.0` |
|         - |  576 | ` */` |
|     24465 |  577 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|         5 |  578 | `{` |
|         - |  579 | `	sxi32 iFlags;` |
|     24470 |  580 | `	iFlags = pObj->iFlags;` |
|     24470 |  581 | `	if( iFlags & MEMOBJ_REAL ){` |
|       ! 0 |  582 | `		return pObj->rVal;` |
|     24470 |  583 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      4165 |  584 | `		return (ph7_real)pObj->x.iVal;` |
|     20310 |  585 | `	}else if (iFlags & MEMOBJ_STRING){` |
|         - |  586 | `		SyString sString;` |
|         - |  587 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  588 | `		ph7_real rVal = 0;` |
|         - |  589 | `#else` |
|     20270 |  590 | `		ph7_real rVal = 0.0;` |
|         - |  591 | `#endif` |
|     20270 |  592 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     20270 |  593 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         - |  594 | `			/* Convert as much as we can */` |
|         - |  595 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  596 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|         - |  597 | `#else` |
|     20268 |  598 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|         - |  599 | `#endif` |
|     10120 |  600 | `		}` |
|     20270 |  601 | `		return rVal;` |
|        45 |  602 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - |  603 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  604 | `		return 0;` |
|         - |  605 | `#else` |
|        15 |  606 | `		return 0.0;` |
|         - |  607 | `#endif` |
|        31 |  608 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|         - |  609 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|         - |  610 | `		 * branch above. */` |
|       ! 0 |  611 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       ! 0 |  612 | `		sxu32 n = pMap->nEntry;` |
|       ! 0 |  613 | `		PH7_HashmapUnref(pMap);` |
|       ! 0 |  614 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|        31 |  615 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  616 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|        29 |  617 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        29 |  618 | `		if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|         3 |  619 | `			ph7_real rV = 0;` |
|         3 |  620 | `			(void)MemObjIntFromClassString(pObj->pVm,pInst,&rV);` |
|         3 |  621 | `			PH7_ClassInstanceUnref(pInst);` |
|         3 |  622 | `			return rV;` |
|         - |  623 | `		}` |
|        26 |  624 | `		if( pInst && pInst->pClass ){` |
|        37 |  625 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        22 |  626 | `				"Object of class %z could not be converted to float",&pInst->pClass->sDisp);` |
|        11 |  627 | `		}` |
|        26 |  628 | `		PH7_ClassInstanceUnref(pInst);` |
|        26 |  629 | `		return (ph7_real)1.0;` |
|         3 |  630 | `	}else if(iFlags & MEMOBJ_RES ){` |
|         3 |  631 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|         - |  632 | `	}` |
|         - |  633 | `	/* NOT REACHED  */` |
|       ! 0 |  634 | `	return 0;` |
|     12226 |  635 | `}` |
|         - |  636 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  637 | `/*` |
|         - |  638 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|         - |  639 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|         - |  640 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|         - |  641 | ` * bGeneric is set (%g-style output, including the default float->string` |
|         - |  642 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|         - |  643 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|         - |  644 | ` * of spare capacity past the NUL. Returns the new length.` |
|         - |  645 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|         - |  646 | ` * even when builtin.c's formatting region is compiled out` |
|         - |  647 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|         - |  648 | ` */` |
|       948 |  649 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|         5 |  650 | `{` |
|         - |  651 | `	sxi32 iExp,i;` |
|       953 |  652 | `	iExp = nLen - 1;` |
|      6895 |  653 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|      5947 |  654 | `		iExp--;` |
|         5 |  655 | `	}` |
|       953 |  656 | `	if( iExp <= 0 ){` |
|       823 |  657 | `		return nLen; /* No exponent part (fixed notation) */` |
|         - |  658 | `	}` |
|         - |  659 | `	{` |
|       133 |  660 | `		sxi32 iDig = iExp + 1;` |
|         - |  661 | `		sxi32 iFirst;` |
|       133 |  662 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|       133 |  663 | `			iDig++;` |
|        65 |  664 | `		}` |
|       133 |  665 | `		iFirst = iDig;` |
|       170 |  666 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|       108 |  667 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|        27 |  668 | `			iFirst++;` |
|         1 |  669 | `		}` |
|       133 |  670 | `		if( iFirst > iDig ){` |
|        27 |  671 | `			sxi32 nStrip = iFirst - iDig;` |
|        79 |  672 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|        53 |  673 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|        27 |  674 | `			}` |
|        27 |  675 | `			nLen -= nStrip;` |
|        13 |  676 | `		}` |
|         - |  677 | `	}` |
|       133 |  678 | `	if( bGeneric ){` |
|       117 |  679 | `		int bHasDot = 0;` |
|       255 |  680 | `		for( i = 0 ; i < iExp ; i++ ){` |
|       211 |  681 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|        72 |  682 | `		}` |
|       117 |  683 | `		if( !bHasDot ){` |
|       277 |  684 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|       233 |  685 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       118 |  686 | `			}` |
|        47 |  687 | `			zBuf[iExp] = '.';` |
|        47 |  688 | `			zBuf[iExp+1] = '0';` |
|        47 |  689 | `			nLen += 2;` |
|        22 |  690 | `		}` |
|        57 |  691 | `	}` |
|       133 |  692 | `	return nLen;` |
|       479 |  693 | `}` |
|         - |  694 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         - |  695 | `/*` |
|         - |  696 | ` * Return the string representation of a given ph7_value.` |
|         - |  697 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|         - |  698 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|         - |  699 | ` * left without a rendering of pObj.` |
|         - |  700 | ` */` |
|     99506 |  701 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|         5 |  702 | `{` |
|     99511 |  703 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - |  704 | `		/* Handle special floating-point values first */` |
|       657 |  705 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|        25 |  706 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|       645 |  707 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|        20 |  708 | `			if( pObj->rVal < 0.0 ){` |
|         3 |  709 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|         2 |  710 | `			}else{` |
|        18 |  711 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|         - |  712 | `			}` |
|        11 |  713 | `		}else{` |
|         - |  714 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |  715 | `			/* php's default float->string conversion (echo/concat/cast):` |
|         - |  716 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|         - |  717 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|         - |  718 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|         - |  719 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|         - |  720 | `			 * exponent/fraction quirks. */` |
|         - |  721 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|       615 |  722 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|       615 |  723 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|       ! 0 |  724 | `				n = (sxi32)SyStrlen(zNum);` |
|       ! 0 |  725 | `			}` |
|       615 |  726 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|       615 |  727 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|         - |  728 | `#else` |
|         - |  729 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|         - |  730 | `#endif` |
|         5 |  731 | `		}` |
|     99185 |  732 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|     93585 |  733 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|         - |  734 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|     52056 |  735 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      1045 |  736 | `		if( bStrictBool ){` |
|         - |  737 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      1045 |  738 | `			if( pObj->x.iVal ){` |
|       241 |  739 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|       118 |  740 | `			}` |
|         - |  741 | `			/* false produces empty string, nothing to append */` |
|       525 |  742 | `		}else{` |
|         - |  743 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|       ! 0 |  744 | `			if( pObj->x.iVal ){` |
|       ! 0 |  745 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|       ! 0 |  746 | `			}else{` |
|       ! 0 |  747 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|         - |  748 | `			}` |
|         5 |  749 | `		}` |
|      4759 |  750 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       378 |  751 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|       378 |  752 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|      4051 |  753 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         - |  754 | `		ph7_value sResult;` |
|         - |  755 | `		sxi32 rc;` |
|         - |  756 | `		/* Invoke the __toString() method if available */` |
|      3376 |  757 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|      3376 |  758 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|         - |  759 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|      3376 |  760 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  761 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|         - |  762 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|         - |  763 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|         - |  764 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|         - |  765 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|         - |  766 | `			 * instance reference. */` |
|       199 |  767 | `			PH7_MemObjRelease(&sResult);` |
|       199 |  768 | `			return rc;` |
|         - |  769 | `		}` |
|      3182 |  770 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|         - |  771 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|         - |  772 | `			 * value, and requiring a non-empty one sent` |
|         - |  773 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|         - |  774 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|         - |  775 | ``			 * result IS a string is the implicit `string` return type on`` |
|         - |  776 | `			 * __toString (installed at its declaration); the fallback below is now` |
|         - |  777 | `			 * reachable only for a class with no __toString at all -- which only` |
|         - |  778 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|         - |  779 | `			 * result no return-type check governs. */` |
|      3178 |  780 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|      1590 |  781 | `		}else{` |
|         - |  782 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|         - |  783 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|         5 |  784 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|         - |  785 | `		}` |
|      3182 |  786 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      3182 |  787 | `		PH7_MemObjRelease(&sResult);` |
|      2082 |  788 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|         - |  789 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|         - |  790 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|        17 |  791 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|         8 |  792 | `	}` |
|     99317 |  793 | `	return SXRET_OK;` |
|     49742 |  794 | `}` |
|         - |  795 | `/*` |
|         - |  796 | ` * Return some kind of boolean value which is the best we can do` |
|         - |  797 | ` * at representing the value that pObj describes as a boolean.` |
|         - |  798 | ` * When converting to boolean, the following values are considered FALSE` |
|         - |  799 | ` * (php's exact set):` |
|         - |  800 | ` * NULL` |
|         - |  801 | ` * the boolean FALSE itself.` |
|         - |  802 | ` * the integer 0 (zero).` |
|         - |  803 | ` * the real 0.0 (zero).` |
|         - |  804 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|         - |  805 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|         - |  806 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|         - |  807 | ` * and were removed under the scope policy PH7-ism policy).` |
|         - |  808 | ` * an array with zero elements.` |
|         - |  809 | ` */` |
|    200440 |  810 | `static sxi32 MemObjIsTruthy(ph7_value *pObj)` |
|         5 |  811 | `{` |
|         - |  812 | `	sxi32 iFlags;` |
|    200445 |  813 | `	iFlags = pObj->iFlags;` |
|    200445 |  814 | `	if (iFlags & MEMOBJ_REAL ){` |
|         - |  815 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|         - |  816 | `		return pObj->rVal ? 1 : 0;` |
|         - |  817 | `#else` |
|         - |  818 | `		/* A NaN is neither zero nor equal to itself, so it is TRUE -- php's` |
|         - |  819 | `		 * answer too, behind the warning PH7_MemObjToBool raises. */` |
|       101 |  820 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|         - |  821 | `#endif` |
|    200347 |  822 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         - |  823 | ``		/* BOOL is here for `empty()`, which asks this of a value of ANY type; the`` |
|         - |  824 | `		 * bool CONVERSION never does (it returns early when the bit is set). */` |
|     37032 |  825 | `		return pObj->x.iVal ? 1 : 0;` |
|    163320 |  826 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|         - |  827 | `		SyString sString;` |
|     40747 |  828 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|         - |  829 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|     40747 |  830 | `		if( sString.nByte == 0 ){` |
|     30761 |  831 | `			return 0;` |
|         - |  832 | `		}` |
|      9991 |  833 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|        26 |  834 | `			return 0;` |
|         - |  835 | `		}` |
|      9967 |  836 | `		return 1;` |
|    122578 |  837 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|     88373 |  838 | `		return 0;` |
|     34210 |  839 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|     31345 |  840 | `		return ((ph7_hashmap *)pObj->x.pOther)->nEntry > 0 ? TRUE : FALSE;` |
|      2870 |  841 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|         - |  842 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|         - |  843 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|         - |  844 | `		 * extension changed control flow in valid php source.` |
|         - |  845 | `		 *` |
|         - |  846 | `		 * An INTERNAL class may still install php's cast_object handler for` |
|         - |  847 | `		 * _IS_BOOL, which is a different thing entirely -- it is not reachable` |
|         - |  848 | `		 * from PHP source and php ships exactly one: a zero BcMath\Number is` |
|         - |  849 | ``		 * falsy, so `if ($n)` and `empty($n)` read the VALUE. */`` |
|       977 |  850 | `		int bNative = 1;` |
|       977 |  851 | `		if( PH7_ClassNativeBool((ph7_class_instance *)pObj->x.pOther,&bNative) ){` |
|        22 |  852 | `			return bNative;` |
|         - |  853 | `		}` |
|       957 |  854 | `		return 1;` |
|      1898 |  855 | `	}else if(iFlags & MEMOBJ_RES ){` |
|      1898 |  856 | `		return pObj->x.pOther != 0;` |
|         - |  857 | `	}` |
|         - |  858 | `	/* NOT REACHED */` |
|       ! 0 |  859 | `	return 0;` |
|    100169 |  860 | `}` |
|         - |  861 | `/*` |
|         - |  862 | ` * The same question asked by a CONVERSION, which is about to overwrite the` |
|         - |  863 | ` * payload and so owes it a reference drop. Nothing else about the answer` |
|         - |  864 | `` * differs -- which is the point: `empty()` and `array_filter()`'s default test`` |
|         - |  865 | ` * used to carry a SECOND set of rules (PH7_MemObjIsEmpty), and it disagreed.` |
|         - |  866 | ` */` |
|    129306 |  867 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|         5 |  868 | `{` |
|    129311 |  869 | `	sxi32 rc = MemObjIsTruthy(&(*pObj));` |
|    129311 |  870 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       208 |  871 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|    129209 |  872 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       967 |  873 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|       479 |  874 | `	}` |
|    129311 |  875 | `	return rc;` |
|         5 |  876 | `}` |
|         - |  877 | `/*` |
|         - |  878 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|         - |  879 | ` */` |
|     28894 |  880 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|         5 |  881 | `{` |
|     28899 |  882 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|         - |  883 | `  /* Only mark the value as an integer if` |
|         - |  884 | `  **` |
|         - |  885 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|         - |  886 | `  **    (2) The integer is neither the largest nor the smallest` |
|         - |  887 | `  **        possible integer` |
|         - |  888 | `  **` |
|         - |  889 | `  ** The second and third terms in the following conditional enforces` |
|         - |  890 | `  ** the second condition under the assumption that addition overflow causes` |
|         - |  891 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|         - |  892 | `  ** true and could be omitted.  But we leave it in because other` |
|         - |  893 | `  ** architectures might behave differently.` |
|         - |  894 | `  */` |
|     28894 |  895 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|     23095 |  896 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|     23069 |  897 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     11522 |  898 | `	}` |
|     28899 |  899 | `	return SXRET_OK;` |
|         5 |  900 | `}` |
|         - |  901 | `/*` |
|         - |  902 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|         - |  903 | ` */` |
|  12398909 |  904 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|         5 |  905 | `{` |
|  12398914 |  906 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  907 | `		/* Preform the conversion */` |
|   1181320 |  908 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|         - |  909 | `		/* Invalidate any prior representations */` |
|   1181320 |  910 | `		SyBlobRelease(&pObj->sBlob);` |
|   1181320 |  911 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|    590640 |  912 | `	}` |
|  12398914 |  913 | `	return SXRET_OK;` |
|         5 |  914 | `}` |
|         - |  915 | `/*` |
|         - |  916 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|         - |  917 | ` * Invalidate any prior representations` |
|         - |  918 | ` */` |
|     28681 |  919 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|         5 |  920 | `{` |
|     28686 |  921 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|         - |  922 | `		/* Preform the conversion */` |
|     24470 |  923 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|         - |  924 | `		/* Invalidate any prior representations */` |
|     24470 |  925 | `		SyBlobRelease(&pObj->sBlob);` |
|     24470 |  926 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|         - |  927 | `		/* Try to get an integer representation */` |
|     24470 |  928 | `		MemObjTryIntger(&(*pObj));` |
|     12221 |  929 | `	}` |
|     28686 |  930 | `	return SXRET_OK;` |
|         5 |  931 | `}` |
|         - |  932 | `/*` |
|         - |  933 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|         - |  934 | ` */` |
|    199468 |  935 | `static sxi32 MemObjToBoolQuiet(ph7_value *pObj)` |
|         5 |  936 | `{` |
|    199473 |  937 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|         - |  938 | `		/* Preform the conversion */` |
|    129311 |  939 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|         - |  940 | `		/* Invalidate any prior representations */` |
|    129311 |  941 | `		SyBlobRelease(&pObj->sBlob);` |
|    129311 |  942 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|     64606 |  943 | `	}` |
|    199473 |  944 | `	return SXRET_OK;` |
|         5 |  945 | `}` |
|         - |  946 | `/*` |
|         - |  947 | ` * The same conversion where a php PROGRAM asked for it, which is every` |
|         - |  948 | `` * truthiness site there is: `(bool)`, `if`, `!`, `&&`, the ternary, `empty()`,`` |
|         - |  949 | `` * `boolval()`, `settype()`, a `bool` parameter internal or userland,`` |
|         - |  950 | `` * `array_filter`'s default test. php 8.5 warns from all of them when the value`` |
|         - |  951 | `` * is a NaN -- `unexpected NAN value was coerced to bool` -- and answers TRUE.`` |
|         - |  952 | ` *` |
|         - |  953 | `` * A COMPARISON is not one of them: `NAN == true` is silent in php, and it`` |
|         - |  954 | ` * reaches the same conversion, which is why the quiet form above exists.` |
|         - |  955 | ` */` |
|    110117 |  956 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|         5 |  957 | `{` |
|    110117 |  958 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     54775 |  959 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        21 |  960 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - |  961 | `			"unexpected NAN value was coerced to bool");` |
|        10 |  962 | `	}` |
|    110122 |  963 | `	return MemObjToBoolQuiet(&(*pObj));` |
|         5 |  964 | `}` |
|         - |  965 | `/*` |
|         - |  966 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|         - |  967 | ` */` |
|  17385767 |  968 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|         5 |  969 | `{` |
|  17385772 |  970 | `	sxi32 rc = SXRET_OK;` |
|  17385772 |  971 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  972 | `		/* Perform the conversion */` |
|     99071 |  973 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|     99071 |  974 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|     99071 |  975 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - |  976 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|         - |  977 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|         - |  978 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|         - |  979 | `			 * leaves $o holding the object after the throw is caught. */` |
|       199 |  980 | `			return rc;` |
|         - |  981 | `		}` |
|     98877 |  982 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     49420 |  983 | `	}` |
|  17385578 |  984 | `	return rc;` |
|   8693398 |  985 | `}` |
|         - |  986 | `/*` |
|         - |  987 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|         - |  988 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|         - |  989 | ` *   Error: Object of class X could not be converted to string` |
|         - |  990 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|         - |  991 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|         - |  992 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|         - |  993 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|         - |  994 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|         - |  995 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|         - |  996 | ` * cast still carried the placeholder.` |
|         - |  997 | ` *` |
|         - |  998 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|         - |  999 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|         - | 1000 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|         - | 1001 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|         - | 1002 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|         - | 1003 | ` * settype() site then blanks its target itself, because php's` |
|         - | 1004 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|         - | 1005 | ` */` |
|       612 | 1006 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|         4 | 1007 | `{` |
|       616 | 1008 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         - | 1009 | `	SyBlob sMsg;` |
|       616 | 1010 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|       616 | 1011 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|       612 | 1012 | `		&pInst->pClass->sDisp);` |
|         - | 1013 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|       616 | 1014 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|         4 | 1015 | `}` |
|         - | 1016 | `/*` |
|         - | 1017 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|         - | 1018 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|         - | 1019 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|         - | 1020 | ` */` |
|    170253 | 1021 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|         5 | 1022 | `{` |
|         - | 1023 | `	ph7_class_instance *pInst;` |
|    170258 | 1024 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|    166025 | 1025 | `		return FALSE;` |
|         - | 1026 | `	}` |
|      4238 | 1027 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      4238 | 1028 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|       ! 0 | 1029 | `		return FALSE;` |
|         - | 1030 | `	}` |
|      4238 | 1031 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|     84795 | 1032 | `}` |
|         - | 1033 | `/*` |
|         - | 1034 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|         - | 1035 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|         - | 1036 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|         - | 1037 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|         - | 1038 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|         - | 1039 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|         - | 1040 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|         - | 1041 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|         - | 1042 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|         - | 1043 | ` *` |
|         - | 1044 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|         - | 1045 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|         - | 1046 | ` * ph7_value carries.` |
|         - | 1047 | ` *` |
|         - | 1048 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|         - | 1049 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|         - | 1050 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|         - | 1051 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|         - | 1052 | ` * exactly as php never throws for them.` |
|         - | 1053 | ` *` |
|         - | 1054 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|         - | 1055 | ` */` |
|  32662912 | 1056 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|         5 | 1057 | `{` |
|  32662917 | 1058 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|  32572897 | 1059 | `		return SXRET_OK;` |
|         - | 1060 | `	}` |
|     90025 | 1061 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|       348 | 1062 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|       170 | 1063 | `	}` |
|         - | 1064 | `	/* php 8.5's other coercion warning, and it rides HERE for the same reason` |
|         - | 1065 | `	 * that one does: this is the conversion a program asked for -- a cast, echo,` |
|         - | 1066 | ``	 * concatenation, interpolation, a `string` parameter -- and not the internal`` |
|         - | 1067 | `	 * one a comparison or a debug renderer makes. A NaN is the only float that` |
|         - | 1068 | `	 * warns; INF and -INF spell themselves out in silence. */` |
|     90025 | 1069 | `	if( (pObj->iFlags & MEMOBJ_REAL) && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        17 | 1070 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,` |
|         - | 1071 | `			"unexpected NAN value was coerced to string");` |
|         8 | 1072 | `	}` |
|     90025 | 1073 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|       616 | 1074 | `		return MemObjThrowNotStringable(pObj);` |
|         - | 1075 | `	}` |
|     89413 | 1076 | `	return PH7_MemObjToString(pObj);` |
|  16329488 | 1077 | `}` |
|         - | 1078 | `/*` |
|         - | 1079 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|         - | 1080 | ` * representation.` |
|         - | 1081 | ` */` |
|         2 | 1082 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|         1 | 1083 | `{` |
|         3 | 1084 | `	return PH7_MemObjRelease(pObj);` |
|         1 | 1085 | `}` |
|         - | 1086 | `/*` |
|         - | 1087 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|         - | 1088 | `  * According to the PHP language reference manual.` |
|         - | 1089 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|         - | 1090 | `  *   to an array results in an array with a single element with index zero` |
|         - | 1091 | `  *   and the value of the scalar which was converted.` |
|         - | 1092 | `  */` |
|     22228 | 1093 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|         5 | 1094 | `{` |
|     22233 | 1095 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 1096 | `		ph7_hashmap *pMap;` |
|         - | 1097 | `		/* Allocate a new hashmap instance */` |
|     21380 | 1098 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|     21380 | 1099 | `		if( pMap == 0 ){` |
|       ! 0 | 1100 | `			return SXERR_MEM;` |
|         - | 1101 | `		}` |
|     21380 | 1102 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|         - | 1103 | `			/*` |
|         - | 1104 | `			 * According to the PHP language reference manual.` |
|         - | 1105 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|         - | 1106 | `			 *   to an array results in an array with a single element with index zero` |
|         - | 1107 | `			 *   and the value of the scalar which was converted.` |
|         - | 1108 | `			 */` |
|       889 | 1109 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       861 | 1110 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       856 | 1111 | `				if( pInst && pObj->pVm->pClosureClass` |
|       861 | 1112 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|         - | 1113 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|         - | 1114 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|         - | 1115 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|         - | 1116 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|         - | 1117 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|         3 | 1118 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         2 | 1119 | `				}else{` |
|         - | 1120 | `					/* Object cast */` |
|       859 | 1121 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|         - | 1122 | `				}` |
|       433 | 1123 | `			}else{` |
|         - | 1124 | `				/* Insert a single element */` |
|        30 | 1125 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|         - | 1126 | `			}` |
|       889 | 1127 | `			SyBlobRelease(&pObj->sBlob);` |
|       442 | 1128 | `		}` |
|         - | 1129 | `		/* Invalidate any prior representation */` |
|     21380 | 1130 | `		PH7_MemObjRelease(pObj);` |
|     21380 | 1131 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|     21380 | 1132 | `		pObj->x.pOther = pMap;` |
|     10640 | 1133 | `	}` |
|     22233 | 1134 | `	return SXRET_OK;` |
|     11024 | 1135 | `}` |
|         - | 1136 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|         - | 1137 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|         - | 1138 | ` * matching PHP) and holding a copy of the value. */` |
|         - | 1139 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|       184 | 1140 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|         5 | 1141 | `{` |
|       189 | 1142 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|         - | 1143 | `	ph7_value *pSlot;` |
|         - | 1144 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|         - | 1145 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|         - | 1146 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|         - | 1147 | `	 * safe to coerce in place. */` |
|       189 | 1148 | `	PH7_MemObjToString(pKey);` |
|       281 | 1149 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|       184 | 1150 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|       189 | 1151 | `	if( pSlot ){` |
|       189 | 1152 | `		PH7_MemObjStore(pValue,pSlot);` |
|        92 | 1153 | `	}` |
|       189 | 1154 | `	return SXRET_OK;` |
|         5 | 1155 | `}` |
|         - | 1156 | `/*` |
|         - | 1157 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|         - | 1158 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|         - | 1159 | ` * matching PHP's (object) cast:` |
|         - | 1160 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|         - | 1161 | ` *   - scalar -> a single property named "scalar".` |
|         - | 1162 | ` *   - null   -> an empty stdClass (no properties).` |
|         - | 1163 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|         - | 1164 | ` */` |
|       118 | 1165 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|         5 | 1166 | `{` |
|       123 | 1167 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 1168 | `		ph7_class_instance *pStd;` |
|         - | 1169 | `		ph7_class *pClass;` |
|         - | 1170 | `		ph7_vm *pVm;` |
|         - | 1171 | `		/* Point to the underlying VM + the stdClass */` |
|       123 | 1172 | `		pVm = pObj->pVm;` |
|       182 | 1173 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|        59 | 1174 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|       123 | 1175 | `		if( pClass == 0 ){` |
|         - | 1176 | `			/* Can't happen,load null instead */` |
|       ! 0 | 1177 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 | 1178 | `			return SXRET_OK;` |
|         - | 1179 | `		}` |
|         - | 1180 | `		/* Instanciate a new (empty) stdClass object */` |
|       123 | 1181 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|       123 | 1182 | `		if( pStd == 0 ){` |
|         - | 1183 | `			/* Out of memory */` |
|       ! 0 | 1184 | `			PH7_MemObjRelease(pObj);` |
|       ! 0 | 1185 | `			return SXRET_OK;` |
|         - | 1186 | `		}` |
|       123 | 1187 | `		pStd->iRef = 1;` |
|       123 | 1188 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1189 | `			/* Array: one dynamic property per entry. */` |
|         - | 1190 | `			struct VmObjCastData sData;` |
|       109 | 1191 | `			sData.pVm = pVm;` |
|       109 | 1192 | `			sData.pStd = pStd;` |
|       109 | 1193 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|        68 | 1194 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 1195 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|        14 | 1196 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|        14 | 1197 | `			if( pSlot ){` |
|        14 | 1198 | `				PH7_MemObjStore(pObj,pSlot);` |
|         6 | 1199 | `			}` |
|         6 | 1200 | `		}` |
|         - | 1201 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|         - | 1202 | `		/* Invalidate any prior representation */` |
|       123 | 1203 | `		PH7_MemObjRelease(pObj);` |
|         - | 1204 | `		/* Save the new instance */` |
|       123 | 1205 | `		pObj->x.pOther = pStd;` |
|       123 | 1206 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|        59 | 1207 | `	}` |
|       123 | 1208 | `	return SXRET_OK;` |
|        64 | 1209 | `}` |
|         - | 1210 | `/*` |
|         - | 1211 | ` * Return a pointer to the appropriate convertion method associated` |
|         - | 1212 | ` * with the given type.` |
|         - | 1213 | ` * Note on type juggling.` |
|         - | 1214 | ` * Accoding to the PHP language reference manual` |
|         - | 1215 | ` *  PHP does not require (or support) explicit type definition in variable` |
|         - | 1216 | ` *  declaration; a variable's type is determined by the context in which` |
|         - | 1217 | ` *  the variable is used. That is to say, if a string value is assigned` |
|         - | 1218 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|         - | 1219 | ` *  assigned to $var, it becomes an integer.` |
|         - | 1220 | ` */` |
|    100438 | 1221 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|         5 | 1222 | `{` |
|    100443 | 1223 | `	if( iFlags & MEMOBJ_STRING ){` |
|       139 | 1224 | `		return PH7_MemObjToString;` |
|    100309 | 1225 | `	}else if( iFlags & MEMOBJ_INT ){` |
|    100163 | 1226 | `		return PH7_MemObjToInteger;` |
|       150 | 1227 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|        81 | 1228 | `		return PH7_MemObjToReal;` |
|        71 | 1229 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|        62 | 1230 | `		return PH7_MemObjToBool;` |
|        11 | 1231 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 1232 | `		return PH7_MemObjToHashmap;` |
|       ! 0 | 1233 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 1234 | `		return PH7_MemObjToObject;` |
|       ! 0 | 1235 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|         - | 1236 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|         - | 1237 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|         - | 1238 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|         - | 1239 | `		 * the parameter default-value path from quietly nulling a non-null` |
|         - | 1240 | `		 * default. */` |
|       ! 0 | 1241 | `		return 0;` |
|         - | 1242 | `	}` |
|         - | 1243 | `	/* NULL cast */` |
|       ! 0 | 1244 | `	return PH7_MemObjToNull;` |
|     50224 | 1245 | `}` |
|         - | 1246 | `/*` |
|         - | 1247 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|         - | 1248 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|         - | 1249 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|         - | 1250 | ` * loose-comparison numeric gate:` |
|         - | 1251 | ` *` |
|         - | 1252 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|         - | 1253 | ` *` |
|         - | 1254 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|         - | 1255 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|         - | 1256 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|         - | 1257 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|         - | 1258 | ` * a non-string value.` |
|         - | 1259 | ` */` |
|         - | 1260 | `/*` |
|         - | 1261 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|         - | 1262 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|         - | 1263 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|         - | 1264 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|         - | 1265 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|         - | 1266 | ` * and rejects a string with no prefix outright.` |
|         - | 1267 | ` */` |
|   1655711 | 1268 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|         5 | 1269 | `{` |
|         - | 1270 | `	const char *z, *zEnd;` |
|         - | 1271 | `	sxu32 n;` |
|   1655716 | 1272 | `	int bDigit = 0;` |
|   1655716 | 1273 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 1274 | `		return 0;` |
|         - | 1275 | `	}` |
|   1655716 | 1276 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|   1655716 | 1277 | `	n = SyBlobLength(&pValue->sBlob);` |
|   1655716 | 1278 | `	if( n == 0 ){` |
|      2047 | 1279 | `		return 0;` |
|         - | 1280 | `	}` |
|   1653674 | 1281 | `	zEnd = z + n;` |
|   1659222 | 1282 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|      5552 | 1283 | `		z++;` |
|         4 | 1284 | `	}` |
|   1653674 | 1285 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      1075 | 1286 | `		z++;` |
|       535 | 1287 | `	}` |
|   4467624 | 1288 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|   2813955 | 1289 | `		z++; bDigit = 1;` |
|         5 | 1290 | `	}` |
|   1653674 | 1291 | `	if( z < zEnd && z[0] == '.' ){` |
|       913 | 1292 | `		z++;` |
|      2429 | 1293 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      1521 | 1294 | `			z++; bDigit = 1;` |
|         5 | 1295 | `		}` |
|       456 | 1296 | `	}` |
|         - | 1297 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|   1653674 | 1298 | `	if( !bDigit ){` |
|    462582 | 1299 | `		return 0;` |
|         - | 1300 | `	}` |
|         - | 1301 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|         - | 1302 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|   1191097 | 1303 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       343 | 1304 | `		const char *zExp = z;` |
|       343 | 1305 | `		z++;` |
|       343 | 1306 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|        23 | 1307 | `			z++;` |
|        11 | 1308 | `		}` |
|       343 | 1309 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|        20 | 1310 | `			z = zExp;` |
|        11 | 1311 | `		}else{` |
|       765 | 1312 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       443 | 1313 | `				z++;` |
|         3 | 1314 | `			}` |
|         - | 1315 | `		}` |
|       170 | 1316 | `	}` |
|   1191097 | 1317 | `	if( pzTail ){` |
|   1191057 | 1318 | `		*pzTail = z;` |
|    595502 | 1319 | `	}` |
|   1191097 | 1320 | `	return 1;` |
|    827832 | 1321 | `}` |
|         - | 1322 | `/*` |
|         - | 1323 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|         - | 1324 | ` * (trailing whitespace allowed, nothing else).` |
|         - | 1325 | ` */` |
|    468668 | 1326 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|         5 | 1327 | `{` |
|    468673 | 1328 | `	const char *zTail = 0, *zEnd;` |
|    468673 | 1329 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|    462346 | 1330 | `		return 0;` |
|         - | 1331 | `	}` |
|      6332 | 1332 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|      6406 | 1333 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|        76 | 1334 | `		zTail++;` |
|         2 | 1335 | `	}` |
|      6332 | 1336 | `	return zTail == zEnd ? 1 : 0;` |
|    234327 | 1337 | `}` |
|         - | 1338 | `/*` |
|         - | 1339 | ` * php's three-way is_numeric_string classification, which only the loose` |
|         - | 1340 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|         - | 1341 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|         - | 1342 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|         - | 1343 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|         - | 1344 | ` * those bytes convert to when it did.` |
|         - | 1345 | ` *` |
|         - | 1346 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|         - | 1347 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|         - | 1348 | ` * however large it is, because it was always going to be a double, so making it` |
|         - | 1349 | ` * one lost no digits.` |
|         - | 1350 | ` *` |
|         - | 1351 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|         - | 1352 | ` * intact when this says no.` |
|         - | 1353 | ` */` |
|       832 | 1354 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|         4 | 1355 | `{` |
|       836 | 1356 | `	const char *zTail = 0;` |
|       836 | 1357 | `	int iOverflow = 0;` |
|       836 | 1358 | `	*piOverflow = 0;` |
|       836 | 1359 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|        94 | 1360 | `		return FALSE;` |
|         - | 1361 | `	}` |
|       744 | 1362 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       ! 0 | 1363 | `		return FALSE;` |
|         - | 1364 | `	}` |
|         - | 1365 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|         - | 1366 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|       744 | 1367 | `	if( MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|        78 | 1368 | `		return FALSE;` |
|         - | 1369 | `	}` |
|       668 | 1370 | `	MemObjStringToInt(pObj,&iOverflow);` |
|       668 | 1371 | `	*piOverflow = iOverflow;` |
|       668 | 1372 | `	if( iOverflow != 0 && prVal ){` |
|       335 | 1373 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|       167 | 1374 | `	}` |
|       668 | 1375 | `	return TRUE;` |
|       416 | 1376 | `}` |
|         - | 1377 | `/*` |
|         - | 1378 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|         - | 1379 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|         - | 1380 | ` * Return TRUE if numeric.FALSE otherwise.` |
|         - | 1381 | ` */` |
|    382936 | 1382 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|         5 | 1383 | `{` |
|    382941 | 1384 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      6073 | 1385 | `		return TRUE;` |
|    376873 | 1386 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|      9733 | 1387 | `		return FALSE;` |
|    367145 | 1388 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 1389 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|    367145 | 1390 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|         - | 1391 | `	}` |
|         - | 1392 | `	/* NOT REACHED */` |
|       ! 0 | 1393 | `	return FALSE;` |
|    191466 | 1394 | `}` |
|         - | 1395 | `/*` |
|         - | 1396 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|         - | 1397 | ` * FALSE otherwise.` |
|         - | 1398 | ` * An ph7_value is considered empty if the following are true:` |
|         - | 1399 | ` * NULL value.` |
|         - | 1400 | ` * Boolean FALSE.` |
|         - | 1401 | ` * Integer/Float with a 0 (zero) value.` |
|         - | 1402 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|         - | 1403 | ` * An empty array.` |
|         - | 1404 | ` * NOTE` |
|         - | 1405 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|         - | 1406 | ` */` |
|     71134 | 1407 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|         5 | 1408 | `{` |
|         - | 1409 | ``	/* php's `empty($x)` is `!zend_is_true($x)` -- the same question the bool cast`` |
|         - | 1410 | `	 * asks, and this used to answer it with rules of its own. They disagreed on a` |
|         - | 1411 | ``	 * string of MORE THAN ONE zero: the old walk called every `"0"` run empty, so`` |
|         - | 1412 | ``	 * `empty("00")` was true and `array_filter(["00"])` dropped it, where php`` |
|         - | 1413 | ``	 * keeps both (only `""` and the single byte `"0"` are false there). The`` |
|         - | 1414 | `	 * warning php raises when a NaN is coerced rides the same door, since` |
|         - | 1415 | ``	 * `empty(NAN)` warns there. */`` |
|     71134 | 1416 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|     35565 | 1417 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         5 | 1418 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 1419 | `			"unexpected NAN value was coerced to bool");` |
|         2 | 1420 | `	}` |
|     71139 | 1421 | `	return !MemObjIsTruthy(&(*pObj));` |
|         5 | 1422 | `}` |
|         - | 1423 | `/*` |
|         - | 1424 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|         - | 1425 | ` * or both.` |
|         - | 1426 | ` * Invalidate any prior representations. Every effort is made to force` |
|         - | 1427 | ` * the conversion, even if the input is a string that does not look` |
|         - | 1428 | ` * completely like a number.Convert as much of the string as we can` |
|         - | 1429 | ` * and ignore the rest.` |
|         - | 1430 | ` */` |
|  27570792 | 1431 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|         5 | 1432 | `{` |
|  27570797 | 1433 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|  27568695 | 1434 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      1438 | 1435 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|       540 | 1436 | `				pObj->x.iVal = 0;` |
|       268 | 1437 | `			}` |
|      1438 | 1438 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|       717 | 1439 | `		}` |
|         - | 1440 | `		/* Already numeric */` |
|  27568695 | 1441 | `		return  SXRET_OK;` |
|         - | 1442 | `	}` |
|      2107 | 1443 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      2103 | 1444 | `		const char *zTail = 0;` |
|      2103 | 1445 | `		int bNum, bReal = 0;` |
|         - | 1446 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|         - | 1447 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|         - | 1448 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|         - | 1449 | `		 * php sees the prefix "1" there and yields int(1). */` |
|      2103 | 1450 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|      2103 | 1451 | `		if( bNum ){` |
|      2067 | 1452 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|      9431 | 1453 | `			while( z < zTail ){` |
|      7553 | 1454 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|       189 | 1455 | `					bReal = 1;` |
|       189 | 1456 | `					break;` |
|         - | 1457 | `				}` |
|      7369 | 1458 | `				z++;` |
|         5 | 1459 | `			}` |
|      1027 | 1460 | `		}` |
|      2103 | 1461 | `		if( bReal ){` |
|       189 | 1462 | `			PH7_MemObjToReal(&(*pObj));` |
|        97 | 1463 | `		}else{` |
|      1919 | 1464 | `			if( !bNum ){` |
|         - | 1465 | `				/* The input does not look at all like a number,set the value to 0 */` |
|        37 | 1466 | `				pObj->x.iVal = 0;` |
|        19 | 1467 | `			}else{` |
|      1883 | 1468 | `				int iOverflow = 0;` |
|         - | 1469 | `				/* Convert as much as we can */` |
|      1883 | 1470 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|         - | 1471 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      1883 | 1472 | `				if( iOverflow ){` |
|         - | 1473 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|         - | 1474 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|         - | 1475 | `					 * inherits that because they all come through here. Clamping it` |
|         - | 1476 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|         - | 1477 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|         - | 1478 | `					 * with no relation to the input. The float is read from the same` |
|         - | 1479 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|         - | 1480 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|         - | 1481 | `					 * saturating, as php's does. The integer-only build has no float` |
|         - | 1482 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|         - | 1483 | `					 * OP_ADD's overflow arm makes there. */` |
|       228 | 1484 | `					PH7_MemObjToReal(&(*pObj));` |
|       228 | 1485 | `					return SXRET_OK;` |
|         - | 1486 | `				}` |
|         - | 1487 | `#endif` |
|         - | 1488 | `			}` |
|      1693 | 1489 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      1693 | 1490 | `			SyBlobRelease(&pObj->sBlob);` |
|         5 | 1491 | `		}` |
|       937 | 1492 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|         5 | 1493 | `		if( pObj->iFlags & MEMOBJ_OBJ ){` |
|         5 | 1494 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|         4 | 1495 | `			if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass)` |
|         5 | 1496 | `			 && PH7_MemObjToString(pObj) == SXRET_OK ){` |
|         - | 1497 | `				/* php's cast_object answers IS_NUMBER from the object's TEXT, and` |
|         - | 1498 | `				 * the text's own shape then decides int or float -- so` |
|         - | 1499 | ``				 * `$xml->price + 0` on `<price>2.5</price>` is 2.5 and not 2. The`` |
|         - | 1500 | `				 * value is a STRING now, so this recursion ends here. */` |
|         5 | 1501 | `				return PH7_MemObjToNumeric(pObj);` |
|         - | 1502 | `			}` |
|       ! 0 | 1503 | `		}` |
|       ! 0 | 1504 | `		PH7_MemObjToInteger(pObj);` |
|       ! 0 | 1505 | `	}else{` |
|         - | 1506 | `		/* Perform a blind cast */` |
|       ! 0 | 1507 | `		PH7_MemObjToReal(&(*pObj));` |
|         - | 1508 | `	}` |
|      1877 | 1509 | `	return SXRET_OK;` |
|  13787938 | 1510 | `}` |
|         - | 1511 | `/*` |
|         - | 1512 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|         - | 1513 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|         - | 1514 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|         - | 1515 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|         - | 1516 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|         - | 1517 | ` * last carried character. Empty strings become "1".` |
|         - | 1518 | ` *` |
|         - | 1519 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|         - | 1520 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|         - | 1521 | ` * a string even though it looks numeric.` |
|         - | 1522 | ` */` |
|       ! 0 | 1523 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|       ! 0 | 1524 | `{` |
|         - | 1525 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|       ! 0 | 1526 | `	enum CarryClass last_class = CARRY_NONE;` |
|         - | 1527 | `	sxu32 nLen, pos;` |
|         - | 1528 | `	sxu8 *zStr;` |
|       ! 0 | 1529 | `	int carry = 1;` |
|         - | 1530 | `	int ch;` |
|         - | 1531 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|         - | 1532 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|         - | 1533 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|         - | 1534 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|         - | 1535 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|       ! 0 | 1536 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 1537 | `		SyBlobNullAppend(&pObj->sBlob);` |
|       ! 0 | 1538 | `	}` |
|       ! 0 | 1539 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|       ! 0 | 1540 | `	if( nLen == 0 ){` |
|       ! 0 | 1541 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|       ! 0 | 1542 | `		return SXRET_OK;` |
|         - | 1543 | `	}` |
|       ! 0 | 1544 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1545 | `	pos = nLen;` |
|       ! 0 | 1546 | `	while( pos > 0 ){` |
|       ! 0 | 1547 | `		pos--;` |
|       ! 0 | 1548 | `		ch = zStr[pos];` |
|       ! 0 | 1549 | `		if( ch >= 'a' && ch <= 'z' ){` |
|       ! 0 | 1550 | `			if( ch == 'z' ){` |
|       ! 0 | 1551 | `				zStr[pos] = 'a';` |
|       ! 0 | 1552 | `				last_class = CARRY_LOWER;` |
|       ! 0 | 1553 | `				continue;` |
|         - | 1554 | `			}` |
|       ! 0 | 1555 | `			zStr[pos]++;` |
|       ! 0 | 1556 | `			carry = 0;` |
|       ! 0 | 1557 | `			break;` |
|       ! 0 | 1558 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|       ! 0 | 1559 | `			if( ch == 'Z' ){` |
|       ! 0 | 1560 | `				zStr[pos] = 'A';` |
|       ! 0 | 1561 | `				last_class = CARRY_UPPER;` |
|       ! 0 | 1562 | `				continue;` |
|         - | 1563 | `			}` |
|       ! 0 | 1564 | `			zStr[pos]++;` |
|       ! 0 | 1565 | `			carry = 0;` |
|       ! 0 | 1566 | `			break;` |
|       ! 0 | 1567 | `		}else if( ch >= '0' && ch <= '9' ){` |
|       ! 0 | 1568 | `			if( ch == '9' ){` |
|       ! 0 | 1569 | `				zStr[pos] = '0';` |
|       ! 0 | 1570 | `				last_class = CARRY_DIGIT;` |
|       ! 0 | 1571 | `				continue;` |
|         - | 1572 | `			}` |
|       ! 0 | 1573 | `			zStr[pos]++;` |
|       ! 0 | 1574 | `			carry = 0;` |
|       ! 0 | 1575 | `			break;` |
|       ! 0 | 1576 | `		}else{` |
|         - | 1577 | `			/* non-alphanumeric: stop without prepending */` |
|       ! 0 | 1578 | `			carry = 0;` |
|       ! 0 | 1579 | `			break;` |
|         - | 1580 | `		}` |
|       ! 0 | 1581 | `	}` |
|       ! 0 | 1582 | `	if( carry ){` |
|         - | 1583 | `		sxu8 prepend;` |
|         - | 1584 | `		sxu32 i;` |
|       ! 0 | 1585 | `		switch( last_class ){` |
|       ! 0 | 1586 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|       ! 0 | 1587 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|       ! 0 | 1588 | `			default:          prepend = (sxu8)'1'; break;` |
|         - | 1589 | `		}` |
|         - | 1590 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|       ! 0 | 1591 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|       ! 0 | 1592 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|       ! 0 | 1593 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 1594 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|       ! 0 | 1595 | `		for( i = nLen - 1; i > 0; i-- ){` |
|       ! 0 | 1596 | `			zStr[i] = zStr[i - 1];` |
|       ! 0 | 1597 | `		}` |
|       ! 0 | 1598 | `		zStr[0] = prepend;` |
|       ! 0 | 1599 | `	}` |
|       ! 0 | 1600 | `	return SXRET_OK;` |
|       ! 0 | 1601 | `}` |
|         - | 1602 | `/*` |
|         - | 1603 | ` * Try a get an integer representation of the given ph7_value.` |
|         - | 1604 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|         - | 1605 | ` */` |
|      4283 | 1606 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|         5 | 1607 | `{` |
|      4288 | 1608 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 1609 | `		/* Work only with reals */` |
|      4288 | 1610 | `		MemObjTryIntger(&(*pObj));` |
|      2131 | 1611 | `	}` |
|      4288 | 1612 | `	return SXRET_OK;` |
|         5 | 1613 | `}` |
|         - | 1614 | `/*` |
|         - | 1615 | ` * Initialize a ph7_value to the null type.` |
|         - | 1616 | ` */` |
|  62720954 | 1617 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|         5 | 1618 | `{` |
|         - | 1619 | `	PHL_VC_NOTE(PHL_VC_INIT,1);` |
|         - | 1620 | `	/* Zero the structure */` |
|  62720959 | 1621 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1622 | `	/* Initialize fields */` |
|  62720959 | 1623 | `	pObj->pVm = pVm;` |
|  62720959 | 1624 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1625 | `	/* Set the NULL type */` |
|  62720959 | 1626 | `	pObj->iFlags = MEMOBJ_NULL;` |
|  62720959 | 1627 | `	return SXRET_OK;` |
|         5 | 1628 | `}` |
|         - | 1629 | `/*` |
|         - | 1630 | ` * Initialize a ph7_value to the integer type.` |
|         - | 1631 | ` */` |
|   8516107 | 1632 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|         5 | 1633 | `{` |
|         - | 1634 | `	/* Zero the structure */` |
|   8516112 | 1635 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1636 | `	/* Initialize fields */` |
|   8516112 | 1637 | `	pObj->pVm = pVm;` |
|   8516112 | 1638 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1639 | `	/* Set the desired type */` |
|   8516112 | 1640 | `	pObj->x.iVal = iVal;` |
|   8516112 | 1641 | `	pObj->iFlags = MEMOBJ_INT;` |
|   8516112 | 1642 | `	return SXRET_OK;` |
|         5 | 1643 | `}` |
|         - | 1644 | `/*` |
|         - | 1645 | ` * Initialize a ph7_value to the boolean type.` |
|         - | 1646 | ` */` |
|     29426 | 1647 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|         5 | 1648 | `{` |
|         - | 1649 | `	/* Zero the structure */` |
|     29431 | 1650 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1651 | `	/* Initialize fields */` |
|     29431 | 1652 | `	pObj->pVm = pVm;` |
|     29431 | 1653 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1654 | `	/* Set the desired type */` |
|     29431 | 1655 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|     29431 | 1656 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|     29431 | 1657 | `	return SXRET_OK;` |
|         5 | 1658 | `}` |
|         - | 1659 | `/*` |
|         - | 1660 | ` * Initialize a ph7_value to the real type.` |
|         - | 1661 | ` */` |
|      1788 | 1662 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|         5 | 1663 | `{` |
|         - | 1664 | `	/* Zero the structure */` |
|      1793 | 1665 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1666 | `	/* Initialize fields */` |
|      1793 | 1667 | `	pObj->pVm = pVm;` |
|      1793 | 1668 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1669 | `	/* Set the desired type */` |
|      1793 | 1670 | `	pObj->rVal = rVal;` |
|      1793 | 1671 | `	pObj->iFlags = MEMOBJ_REAL;` |
|      1793 | 1672 | `	return SXRET_OK;` |
|         5 | 1673 | `}` |
|         - | 1674 | `/*` |
|         - | 1675 | ` * Initialize a ph7_value to the array type.` |
|         - | 1676 | ` */` |
|   4822759 | 1677 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|         5 | 1678 | `{` |
|         - | 1679 | `	/* Zero the structure */` |
|   4822764 | 1680 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1681 | `	/* Initialize fields */` |
|   4822764 | 1682 | `	pObj->pVm = pVm;` |
|   4822764 | 1683 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|         - | 1684 | `	/* Set the desired type */` |
|   4822764 | 1685 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|   4822764 | 1686 | `	pObj->x.pOther = pArray;` |
|   4822764 | 1687 | `	return SXRET_OK;` |
|         5 | 1688 | `}` |
|         - | 1689 | `/*` |
|         - | 1690 | ` * Initialize a ph7_value to the string type.` |
|         - | 1691 | ` */` |
|  12913731 | 1692 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|         5 | 1693 | `{` |
|         - | 1694 | `	/* Zero the structure */` |
|  12913736 | 1695 | `	SyZero(pObj,sizeof(ph7_value));` |
|         - | 1696 | `	/* Initialize fields */` |
|  12913736 | 1697 | `	pObj->pVm = pVm;` |
|  12913736 | 1698 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|  12913736 | 1699 | `	if( pVal ){` |
|         - | 1700 | `		/* Append contents */` |
|   8643198 | 1701 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|   4320668 | 1702 | `	}` |
|         - | 1703 | `	/* Set the desired type */` |
|  12913736 | 1704 | `	pObj->iFlags = MEMOBJ_STRING;` |
|  12913736 | 1705 | `	return SXRET_OK;` |
|         5 | 1706 | `}` |
|         - | 1707 | `/*` |
|         - | 1708 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|         - | 1709 | ` * If the given ph7_value is not of type string,this function` |
|         - | 1710 | ` * invalidate any prior representation and set the string type.` |
|         - | 1711 | ` * Then a simple append operation is performed.` |
|         - | 1712 | ` */` |
|  20569279 | 1713 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|         5 | 1714 | `{` |
|         - | 1715 | `	sxi32 rc;` |
|  20569284 | 1716 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1717 | `		/* Invalidate any prior representation */` |
|    120577 | 1718 | `		PH7_MemObjRelease(pObj);` |
|    120577 | 1719 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|     59879 | 1720 | `	}` |
|         - | 1721 | `	/* Append contents */` |
|  20569284 | 1722 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  20569284 | 1723 | `	return rc;` |
|         5 | 1724 | `}` |
|         - | 1725 | `#if 0` |
|         - | 1726 | `/*` |
|         - | 1727 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|         - | 1728 | ` * If the given ph7_value is not of type string,this function invalidate` |
|         - | 1729 | ` * any prior representation and set the string type.` |
|         - | 1730 | ` * Then a simple format and append operation is performed.` |
|         - | 1731 | ` */` |
|         - | 1732 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|         - | 1733 | `{` |
|         - | 1734 | `	sxi32 rc;` |
|         - | 1735 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - | 1736 | `		/* Invalidate any prior representation */` |
|         - | 1737 | `		PH7_MemObjRelease(pObj);` |
|         - | 1738 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|         - | 1739 | `	}` |
|         - | 1740 | `	/* Format and append contents */` |
|         - | 1741 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|         - | 1742 | `	return rc;` |
|         - | 1743 | `}` |
|         - | 1744 | `#endif` |
|         - | 1745 | `/*` |
|         - | 1746 | ` * Duplicate the contents of a ph7_value.` |
|         - | 1747 | ` */` |
|  54768587 | 1748 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|         5 | 1749 | `{` |
|  54768592 | 1750 | `	ph7_class_instance *pObj = 0;` |
|  54768592 | 1751 | `	ph7_hashmap *pMap = 0;` |
|  54768592 | 1752 | `	void *pRes = 0;` |
|         - | 1753 | `	sxi32 rc;` |
|         - | 1754 | `	PHL_VC_NOTE(PHL_VC_STORE,(pSrc->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0);` |
|  54768592 | 1755 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1756 | `		/* Increment reference count */` |
|   4459347 | 1757 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
|  52538557 | 1758 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|         - | 1759 | `		/* Increment reference count */` |
|    329993 | 1760 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|  50144073 | 1761 | `	}else if( pSrc->iFlags & MEMOBJ_STREAMRES ){` |
|         - | 1762 | `		/* One more holder of the stream handle -- see io_private.nValRef. Taken` |
|         - | 1763 | ``		 * BEFORE the destination drops its own, so `$h = $h` cannot close it. */`` |
|    168120 | 1764 | `		PH7_StreamValueRef(pSrc->x.pOther);` |
|     83963 | 1765 | `	}` |
|  54768592 | 1766 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|    314426 | 1767 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
|  54611318 | 1768 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|     98415 | 1769 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|  54404941 | 1770 | `	}else if( pDest->iFlags & MEMOBJ_STREAMRES ){` |
|    235002 | 1771 | `		pRes = pDest->x.pOther;` |
|    116766 | 1772 | `	}` |
|  54768592 | 1773 | `	PH7_MEMOBJ_COPY_SCALAR(pDest,pSrc);` |
|  54768592 | 1774 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
|  54768592 | 1775 | `	rc = SXRET_OK;` |
|  54768592 | 1776 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
|  33005905 | 1777 | `		SyBlobReset(&pDest->sBlob);` |
|  33005905 | 1778 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  16498936 | 1779 | `	}else{` |
|  21762692 | 1780 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|   7607052 | 1781 | `			SyBlobRelease(&pDest->sBlob);` |
|   3805076 | 1782 | `		}` |
|         - | 1783 | `	}` |
|  54768592 | 1784 | `	if( pMap ){` |
|    314426 | 1785 | `		PH7_HashmapUnref(pMap);` |
|  54611318 | 1786 | `	}else if( pObj ){` |
|     98415 | 1787 | `		PH7_ClassInstanceUnref(pObj);` |
|  54404941 | 1788 | `	}else if( pRes ){` |
|         - | 1789 | `		/* The handle the destination used to name loses this value. */` |
|    235002 | 1790 | `		PH7_StreamValueUnref(pRes);` |
|    116766 | 1791 | `	}` |
|  54768587 | 1792 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
|  29611969 | 1793 | `	 && pDest->pVm` |
|   4459342 | 1794 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|         - | 1795 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|         - | 1796 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|         - | 1797 | `	  * for closure envs and other non-slot destinations. */` |
|   2229316 | 1798 | `	 && pDest != (ph7_value *)PH7_MemObjAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|         - | 1799 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|         - | 1800 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|         - | 1801 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|         - | 1802 | `		 * flattened — never a live alias. Materialize it here, the one` |
|         - | 1803 | `		 * store choke point (loads/subscript access keep sharing, so` |
|         - | 1804 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|         9 | 1805 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|         9 | 1806 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|         9 | 1807 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|         9 | 1808 | `			pDest->x.pOther = pSnap;` |
|         4 | 1809 | `		}else if( pSnap ){` |
|       ! 0 | 1810 | `			PH7_HashmapUnref(pSnap);` |
|       ! 0 | 1811 | `		}` |
|         4 | 1812 | `	}` |
|  54768592 | 1813 | `	return rc;` |
|         5 | 1814 | `}` |
|         - | 1815 | `/*` |
|         - | 1816 | ` * Read a value WITHOUT converting the caller's copy of it.` |
|         - | 1817 | ` *` |
|         - | 1818 | ` * Every ph7_value_to_xxx()/PH7_MemObjToXxx() is destructive: it rewrites the` |
|         - | 1819 | ` * object it is handed and throws the prior representation away. That is right` |
|         - | 1820 | ` * for a VM operand, and wrong for an entry a builtin FETCHED out of an array` |
|         - | 1821 | ` * the script still holds — an $options member, a stream-filter parameter, a` |
|         - | 1822 | ` * proc_open descriptor — where converting in place rewrites the script's own` |
|         - | 1823 | ` * array (php's zval_get_long()/zval_get_string() family never touch theirs).` |
|         - | 1824 | ` *` |
|         - | 1825 | ` * PH7_ValuePeek loads an aliasing copy into pScratch (which the caller must` |
|         - | 1826 | ` * have PH7_MemObjInit'd and must PH7_MemObjRelease afterwards) and answers it,` |
|         - | 1827 | ` * so the destructive conversion lands on the copy. A string read through it` |
|         - | 1828 | ` * stays valid until the scratch value is released. The three scalar wrappers` |
|         - | 1829 | ` * carry their own scratch for the common case.` |
|         - | 1830 | ` */` |
|      2750 | 1831 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch)` |
|         3 | 1832 | `{` |
|      2753 | 1833 | `	PH7_MemObjLoad(pVal,pScratch);` |
|      2753 | 1834 | `	return pScratch;` |
|         3 | 1835 | `}` |
|      1748 | 1836 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal)` |
|         5 | 1837 | `{` |
|         - | 1838 | `	ph7_value sTmp;` |
|         - | 1839 | `	sxi64 iVal;` |
|      1753 | 1840 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|      1753 | 1841 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|      1753 | 1842 | `	PH7_MemObjToInteger(&sTmp);` |
|      1753 | 1843 | `	iVal = sTmp.x.iVal;` |
|      1753 | 1844 | `	PH7_MemObjRelease(&sTmp);` |
|      1753 | 1845 | `	return iVal;` |
|         5 | 1846 | `}` |
|         - | 1847 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       524 | 1848 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal)` |
|         2 | 1849 | `{` |
|         - | 1850 | `	ph7_value sTmp;` |
|         - | 1851 | `	ph7_real rVal;` |
|       526 | 1852 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|       526 | 1853 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|       526 | 1854 | `	PH7_MemObjToReal(&sTmp);` |
|       526 | 1855 | `	rVal = sTmp.rVal;` |
|       526 | 1856 | `	PH7_MemObjRelease(&sTmp);` |
|       526 | 1857 | `	return rVal;` |
|         2 | 1858 | `}` |
|         - | 1859 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|         6 | 1860 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal)` |
|         2 | 1861 | `{` |
|         - | 1862 | `	ph7_value sTmp;` |
|         - | 1863 | `	int bVal;` |
|         8 | 1864 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|         8 | 1865 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|         8 | 1866 | `	PH7_MemObjToBool(&sTmp);` |
|         8 | 1867 | `	bVal = sTmp.x.iVal != 0;` |
|         8 | 1868 | `	PH7_MemObjRelease(&sTmp);` |
|         8 | 1869 | `	return bVal;` |
|         2 | 1870 | `}` |
|         - | 1871 | `/*` |
|         - | 1872 | ` * Invalidate any prior representation of a given ph7_value.` |
|         - | 1873 | ` *` |
|         - | 1874 | ` * The SLOW half of PH7_MemObjRelease (ph7int.h), which is the door every caller` |
|         - | 1875 | ` * still writes. This body runs only for a value that actually owns something: a` |
|         - | 1876 | ` * hashmap or instance reference, a string blob, or one of the three AUX carriers.` |
|         - | 1877 | ` * The inline guard turns the other 43.1% of the engine's 2.72 billion releases` |
|         - | 1878 | ` * into a test. Nothing below may act on a value that is MEMOBJ_NULL and carries` |
|         - | 1879 | ` * no AUX bit -- that combination never reaches here.` |
|         - | 1880 | ` */` |
| 274306426 | 1881 | `PH7_PRIVATE sxi32 PH7_MemObjReleaseSlow(ph7_value *pObj)` |
|         5 | 1882 | `{` |
|         - | 1883 | `	/* The array-literal position markers die with the slot's contents: they describe` |
|         - | 1884 | `	 * where this value sat in the literal LOAD_MAP has now read, and the next value to` |
|         - | 1885 | `	 * occupy the slot sits somewhere else in some other literal. Cleared first, ahead of` |
|         - | 1886 | `	 * every early return below -- see MEMOBJ_AUX_STACKMARK. */` |
| 274306431 | 1887 | `	pObj->iFlags &= ~MEMOBJ_AUX_STACKMARK;` |
| 274306431 | 1888 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|         - | 1889 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|         - | 1890 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|         - | 1891 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|         - | 1892 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|         - | 1893 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|         7 | 1894 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|         7 | 1895 | `		pObj->x.pOther = 0;` |
|         7 | 1896 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|         3 | 1897 | `	}` |
| 274306431 | 1898 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|         - | 1899 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|         - | 1900 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|         - | 1901 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|         - | 1902 | `		 * argument list threw never leaks the receiver it was holding. */` |
|       ! 0 | 1903 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|       ! 0 | 1904 | `		pObj->x.pOther = 0;` |
|       ! 0 | 1905 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|       ! 0 | 1906 | `	}` |
| 274306431 | 1907 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|         - | 1908 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|         - | 1909 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|         - | 1910 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|         - | 1911 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|         3 | 1912 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|         3 | 1913 | `		pObj->x.pOther = 0;` |
|         3 | 1914 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|         1 | 1915 | `	}` |
| 274306431 | 1916 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 273518049 | 1917 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|  10280705 | 1918 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 268376818 | 1919 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|   5516962 | 1920 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
| 260478296 | 1921 | `		}else if( pObj->iFlags & MEMOBJ_STREAMRES ){` |
|         - | 1922 | `			/* The last value naming a stream handle closes it, as php's does. */` |
|    230523 | 1923 | `			PH7_StreamValueUnref(pObj->x.pOther);` |
|    115147 | 1924 | `		}` |
|         - | 1925 | `		/* Release the internal buffer */` |
| 273518049 | 1926 | `		SyBlobRelease(&pObj->sBlob);` |
|         - | 1927 | `		/* Invalidate any prior representation */` |
| 273518049 | 1928 | `		pObj->iFlags = MEMOBJ_NULL;` |
| 136771320 | 1929 | `	}` |
| 274306431 | 1930 | `	return SXRET_OK;` |
|         5 | 1931 | `}` |
|         - | 1932 | `/*` |
|         - | 1933 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|         - | 1934 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|         - | 1935 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|         - | 1936 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|         - | 1937 | ` * caller reports the object as greater, exactly as php does.` |
|         - | 1938 | ` *` |
|         - | 1939 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|         - | 1940 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|         - | 1941 | ` * is always initialized, so the caller can release it either way.` |
|         - | 1942 | ` *` |
|         - | 1943 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|         - | 1944 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|         - | 1945 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|         - | 1946 | ` * different places with different severities, so this one is emitted here rather` |
|         - | 1947 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|         - | 1948 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|         - | 1949 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|         - | 1950 | ` */` |
|       248 | 1951 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|         5 | 1952 | `{` |
|       253 | 1953 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|       253 | 1954 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|       253 | 1955 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|       168 | 1956 | `		if( PH7_MemObjIsNotStringable(pSelf)` |
|       157 | 1957 | `		 \|\| (pSelf->pVm && PH7_CALLBACK_UNWOUND(pSelf->pVm->nBoundaryRc)) ){` |
|         - | 1958 | `			/* php enters no PHP function while an exception is pending` |
|         - | 1959 | `			 * (zend_call_function bails on EG(exception)), so a __toString()` |
|         - | 1960 | `			 * that already threw -- or exited -- is NOT run again: every later` |
|         - | 1961 | `			 * comparison orders this operand the way a refused cast does. A sort` |
|         - | 1962 | `			 * used to re-enter the body once per remaining pair, and where the` |
|         - | 1963 | `			 * enclosing catch had already run in place the second throw was` |
|         - | 1964 | `			 * UNCAUGHT and killed the script. */` |
|        77 | 1965 | `			return FALSE;` |
|         - | 1966 | `		}` |
|        98 | 1967 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        98 | 1968 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|         - | 1969 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|         - | 1970 | `			 * point; until then order the operands the way a refused cast does. */` |
|        39 | 1971 | `			return FALSE;` |
|         - | 1972 | `		}` |
|        59 | 1973 | `		return TRUE;` |
|         - | 1974 | `	}` |
|        84 | 1975 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|         - | 1976 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|        25 | 1977 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|        25 | 1978 | `		return TRUE;` |
|         - | 1979 | `	}` |
|        58 | 1980 | `	if( (pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL))` |
|        45 | 1981 | `	 && pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|         - | 1982 | `		/* A class whose cast_object really answers a number: php compares against` |
|         - | 1983 | ``		 * THAT, silently -- `(string)$xml->n == 5` and `$xml->n == 5` agree. */`` |
|         5 | 1984 | `		PH7_MemObjLoad(pSelf,pOut);` |
|         5 | 1985 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|       ! 0 | 1986 | `			return FALSE;` |
|         - | 1987 | `		}` |
|         5 | 1988 | `		PH7_MemObjToNumeric(pOut);` |
|         5 | 1989 | `		return TRUE;` |
|         - | 1990 | `	}` |
|        57 | 1991 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|        21 | 1992 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|        21 | 1993 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|        31 | 1994 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|         - | 1995 | `				"Object of class %z could not be converted to %s",` |
|        20 | 1996 | `				&pInst->pClass->sDisp,bReal ? "float" : "int");` |
|        10 | 1997 | `		}` |
|        21 | 1998 | `		if( bReal ){` |
|         7 | 1999 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|         4 | 2000 | `		}else{` |
|        15 | 2001 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|         - | 2002 | `		}` |
|        21 | 2003 | `		return TRUE;` |
|         - | 2004 | `	}` |
|        37 | 2005 | `	return FALSE;` |
|       129 | 2006 | `}` |
|         - | 2007 | `/*` |
|         - | 2008 | ` * Compare two ph7_values.` |
|         - | 2009 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|         - | 2010 | ` * or < 0 if pObj2 is greater than pObj1.` |
|         - | 2011 | ` * Type comparison table taken from the PHP language reference manual.` |
|         - | 2012 | ` * Comparisons of $x with PHP functions Expression` |
|         - | 2013 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|         - | 2014 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 2015 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 2016 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 2017 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 2018 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 2019 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 2020 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2021 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2022 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2023 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 2024 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2025 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2026 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|         - | 2027 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2028 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2029 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2030 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|         - | 2031 | ` *      Loose comparisons with ==` |
|         - | 2032 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 2033 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 2034 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 2035 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2036 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|         - | 2037 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2038 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2039 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2040 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2041 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|         - | 2042 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|         - | 2043 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 2044 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|         - | 2045 | ` *    Strict comparisons with ===` |
|         - | 2046 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|         - | 2047 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2048 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2049 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2050 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2051 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2052 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2053 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2054 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|         - | 2055 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|         - | 2056 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|         - | 2057 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|         - | 2058 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|         - | 2059 | ` */` |
|  39101896 | 2060 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|         5 | 2061 | `{` |
|         - | 2062 | `	sxi32 iComb;` |
|         - | 2063 | `	sxi32 rc;` |
|  39101901 | 2064 | `	if( bStrict ){` |
|         - | 2065 | `		sxi32 iF1,iF2;` |
|         - | 2066 | `		/* Strict comparisons with === */` |
|   9378158 | 2067 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|   9378158 | 2068 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|         - | 2069 | `		/* MEMOBJ_INT beside MEMOBJ_REAL is not a TYPE: it is the speculative` |
|         - | 2070 | `		 * integer MemObjTryIntger leaves on a float whose value happens to be a` |
|         - | 2071 | `		 * whole number, and whether a given float carries one depends on which` |
|         - | 2072 | ``		 * road it took here. A literal `1.0` is built through PH7_MemObjToReal`` |
|         - | 2073 | ``		 * and arrives REAL\|INT; `(float)"1.0"` goes through OP_CVT_REAL, whose`` |
|         - | 2074 | `		 * MemObjSetType wipes the speculation, and arrives REAL. Comparing the` |
|         - | 2075 | ``		 * raw flags then made `(float)"1.0" === 1.0` FALSE while`` |
|         - | 2076 | ``		 * `(float)"2.5" === 2.5` was true -- the same wrong answer through`` |
|         - | 2077 | ``		 * in_array($x,[...],true), array_search(), an array `===` and every`` |
|         - | 2078 | `		 * other door that asks this comparator for identity. php has one float` |
|         - | 2079 | `		 * type, so drop the speculation before the type test; the numeric branch` |
|         - | 2080 | `		 * below already compares as reals whenever either side is one. */` |
|   9378158 | 2081 | `		if( iF1 & MEMOBJ_REAL ){ iF1 &= ~MEMOBJ_INT; }` |
|   9378158 | 2082 | `		if( iF2 & MEMOBJ_REAL ){ iF2 &= ~MEMOBJ_INT; }` |
|   9378158 | 2083 | `		if( iF1 != iF2 ){` |
|         - | 2084 | `			/* Not of the same type */` |
|    591700 | 2085 | `			return 1;` |
|         - | 2086 | `		}` |
|   4395111 | 2087 | `	}` |
|         - | 2088 | `	/* Combine flag together */` |
|  38510206 | 2089 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  38510201 | 2090 | `	if( !bStrict` |
|  34118854 | 2091 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|  14864537 | 2092 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|        95 | 2093 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|         - | 2094 | `		/*` |
|         - | 2095 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|         - | 2096 | `		 * compared as the empty string (a string comparison), not through` |
|         - | 2097 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|         - | 2098 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|         - | 2099 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|         - | 2100 | `		 * Convert the null side to "" and let the string branch below run.` |
|         - | 2101 | `		 */` |
|        45 | 2102 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|        31 | 2103 | `			PH7_MemObjToString(pObj1);` |
|        16 | 2104 | `		}else{` |
|        15 | 2105 | `			PH7_MemObjToString(pObj2);` |
|         - | 2106 | `		}` |
|        45 | 2107 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|        22 | 2108 | `	}` |
|  38510206 | 2109 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|         - | 2110 | `		/* php compares two resources by their ID. The boolean path below would` |
|         - | 2111 | `		 * call every live resource equal to every other, since all are truthy. */` |
|        62 | 2112 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|        62 | 2113 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|        62 | 2114 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|         - | 2115 | `	}` |
|  38510148 | 2116 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|         - | 2117 | `		/*` |
|         - | 2118 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|         - | 2119 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|         - | 2120 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|         - | 2121 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|         - | 2122 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|         - | 2123 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|         - | 2124 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|         - | 2125 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|         - | 2126 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|         - | 2127 | `		 * equalled every open resource.` |
|         - | 2128 | `		 *` |
|         - | 2129 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|         - | 2130 | `		 * already answered 1.` |
|         - | 2131 | `		 */` |
|       347 | 2132 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|       347 | 2133 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|       347 | 2134 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|         - | 2135 | `		ph7_value sCast;` |
|         - | 2136 | `		{` |
|         - | 2137 | `			/* ...unless the object's class declares php's compare handler, which is` |
|         - | 2138 | ``			 * asked about a scalar partner too: `Number('1.5') == '1.50'` is TRUE`` |
|         - | 2139 | `			 * where the cast rule would compare two strings. A handler that does not` |
|         - | 2140 | `			 * recognize the value falls through to the cast below. */` |
|       347 | 2141 | `			sxi32 iNative = 1;` |
|       518 | 2142 | `			if( PH7_ClassNativeCmpValue((ph7_class_instance *)pSelf->x.pOther,pOther,` |
|       171 | 2143 | `				!bObj1,&iNative) ){` |
|         - | 2144 | `				/* The hook was told which side it is on and has already flipped its` |
|         - | 2145 | `				 * ordering; the uncomparable 1 is deliberately NOT flipped, which is` |
|         - | 2146 | `				 * what leaves every relational spelling false from both directions. */` |
|        97 | 2147 | `				return iNative;` |
|         - | 2148 | `			}` |
|         - | 2149 | `		}` |
|       253 | 2150 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|         - | 2151 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|        98 | 2152 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|        59 | 2153 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|       106 | 2154 | `			PH7_MemObjRelease(&sCast);` |
|       106 | 2155 | `			return rc;` |
|         - | 2156 | `		}` |
|       151 | 2157 | `		PH7_MemObjRelease(&sCast);` |
|         - | 2158 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|       151 | 2159 | `		return bObj1 ? 1 : -1;` |
|         - | 2160 | `	}` |
|  38509806 | 2161 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|         - | 2162 | `		/* Convert to boolean: Keep in mind FALSE < TRUE. php decides null and bool` |
|         - | 2163 | `		 * this way and nothing else -- a RESOURCE used to be decided here too,` |
|         - | 2164 | `		 * which made every open one equal to every other truthy value. */` |
|     84262 | 2165 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     45582 | 2166 | `			MemObjToBoolQuiet(pObj1);   /* php's comparison says nothing about a NaN */` |
|     22769 | 2167 | `		}` |
|     84262 | 2168 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     43779 | 2169 | `			MemObjToBoolQuiet(pObj2);` |
|     21870 | 2170 | `		}` |
|     84262 | 2171 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  38425549 | 2172 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|         - | 2173 | `		/* Hashmap aka 'array' comparison */` |
|      4012 | 2174 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2175 | `			/* Array is always greater */` |
|        37 | 2176 | `			return -1;` |
|         - | 2177 | `		}` |
|      3976 | 2178 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2179 | `			/* Array is always greater */` |
|        21 | 2180 | `			return 1;` |
|         - | 2181 | `		}` |
|         - | 2182 | `		/* Perform the comparison */` |
|      3956 | 2183 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict,iNest+1);` |
|      3956 | 2184 | `		return rc;` |
|  38421542 | 2185 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|         - | 2186 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|         - | 2187 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|         - | 2188 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|         - | 2189 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|      1547 | 2190 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 2191 | `			/* Object is always greater */` |
|       ! 0 | 2192 | `			return -1;` |
|         - | 2193 | `		}` |
|      1547 | 2194 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|         - | 2195 | `			/* Object is always greater */` |
|       ! 0 | 2196 | `			return 1;` |
|         - | 2197 | `		}` |
|         - | 2198 | `		/* Perform the comparison */` |
|      1547 | 2199 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      1547 | 2200 | `		return rc;` |
|  38420000 | 2201 | `	}else if( !VmIsUnorderedCmp(pObj1,pObj2) && (iComb & MEMOBJ_RES) ){` |
|         - | 2202 | `		/* php compares a resource with a NON-resource as its ID -- the number` |
|         - | 2203 | ``		 * `(int)$fp` answers -- and the other side takes php's LEGACY`` |
|         - | 2204 | `		 * scalar-to-number conversion, not php 8's saner string rule, because` |
|         - | 2205 | ``		 * this comparison never reaches that rule: `$fp == "5abc"` is TRUE for`` |
|         - | 2206 | ``		 * resource #5 and `$fp > "x"` compares 5 with 0. PHL compared the pair as`` |
|         - | 2207 | `		 * BOOLEANS, so an open resource equalled every non-empty string, every` |
|         - | 2208 | `		 * non-zero number and every other open resource, was GREATER than the` |
|         - | 2209 | ``		 * empty array, and `max($fp, 10)` answered the resource.`` |
|         - | 2210 | `		 *` |
|         - | 2211 | `		 * The two-resource case is decided above (by ID), null and bool before` |
|         - | 2212 | `		 * that (php's bool comparison), an array above this (an array is` |
|         - | 2213 | `		 * greater), an object by the cast rule, and a NaN by the unordered one --` |
|         - | 2214 | `		 * exactly php's order. */` |
|       227 | 2215 | `		int bRes1 = (pObj1->iFlags & MEMOBJ_RES) != 0;` |
|       227 | 2216 | `		ph7_value *pRes = bRes1 ? pObj1 : pObj2;` |
|       227 | 2217 | `		ph7_value *pOther = bRes1 ? pObj2 : pObj1;` |
|       227 | 2218 | `		sxi64 iId = (sxi64)PH7_VmResourceId(pRes->pVm,pRes->x.pOther);` |
|       227 | 2219 | `		PH7_MemObjToNumeric(pOther);` |
|       227 | 2220 | `		if( pOther->iFlags & MEMOBJ_REAL ){` |
|        57 | 2221 | `			ph7_real rId = (ph7_real)iId;` |
|        57 | 2222 | `			rc = rId > pOther->rVal ? 1 : (rId < pOther->rVal ? -1 : 0);` |
|        29 | 2223 | `		}else{` |
|       171 | 2224 | `			rc = iId > pOther->x.iVal ? 1 : (iId < pOther->x.iVal ? -1 : 0);` |
|         - | 2225 | `		}` |
|       227 | 2226 | `		return bRes1 ? rc : -rc;` |
|  38419774 | 2227 | `	}else if( VmIsUnorderedCmp(pObj1,pObj2) ){` |
|         - | 2228 | `		/* A NaN against a number or a string: php answers 1 in BOTH directions` |
|         - | 2229 | ``		 * (`NAN <=> 1` and `1 <=> NAN` are both 1), which is what leaves every`` |
|         - | 2230 | `		 * relational operator false at once. The rule lives HERE, not only in the` |
|         - | 2231 | `		 * operator arms, because everything else that orders values goes through` |
|         - | 2232 | ``		 * this comparator with no arm of its own: `in_array(NAN, ["NAN"])` was TRUE`` |
|         - | 2233 | ``		 * (php: false), `array_search` found it, and a `switch` matched it -- all`` |
|         - | 2234 | `		 * because the string branch below rendered the NaN as the bytes "NAN" and` |
|         - | 2235 | `		 * compared those. The precedence php gives null, bool, array and object is` |
|         - | 2236 | `		 * already spent above: VmIsUnorderedCmp screens those flags out, so` |
|         - | 2237 | ``		 * `NAN == true` stays the bool comparison it is there. */`` |
|       278 | 2238 | `		return 1;` |
|  38419498 | 2239 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|         - | 2240 | `		SyString s1,s2;` |
|   4154125 | 2241 | `		if( !bStrict ){` |
|         - | 2242 | `			/*` |
|         - | 2243 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|         - | 2244 | `			 * comparison is performed only when BOTH operands are numbers or` |
|         - | 2245 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|         - | 2246 | `			 * compared as strings, with the number cast to its string form —` |
|         - | 2247 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|         - | 2248 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|         - | 2249 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|         - | 2250 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|         - | 2251 | `			 * non-numeric string, still fall through to the string comparison` |
|         - | 2252 | `			 * below, unchanged.` |
|         - | 2253 | `			 */` |
|    358267 | 2254 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|         - | 2255 | `				/*` |
|         - | 2256 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|         - | 2257 | `				 * compared through their doubles, because the conversion threw away` |
|         - | 2258 | `				 * the digits that tell them apart. php has two rules for them, both` |
|         - | 2259 | `				 * only for a string against a string (a string against an int VALUE` |
|         - | 2260 | `				 * really does compare as doubles, so` |
|         - | 2261 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|         - | 2262 | `				 *` |
|         - | 2263 | `				 *  - Same side, same double: compare the BYTES. So` |
|         - | 2264 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|         - | 2265 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|         - | 2266 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|         - | 2267 | `				 *    digit runs that both overflow to infinity land here as well.` |
|         - | 2268 | `				 *  - One side past the range, the other an integer-shaped string that` |
|         - | 2269 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|         - | 2270 | `				 *    one, no conversion involved -- which is why` |
|         - | 2271 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|         - | 2272 | `				 *    reach the same double.` |
|         - | 2273 | `				 *` |
|         - | 2274 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|         - | 2275 | `				 * float-SHAPED operand, or anything that is not a string.` |
|         - | 2276 | `				 */` |
|       420 | 2277 | `				int bBytes = 0;` |
|         - | 2278 | `				{` |
|       420 | 2279 | `					ph7_real r1 = 0, r2 = 0;` |
|       420 | 2280 | `					int iOf1 = 0, iOf2 = 0;` |
|       420 | 2281 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|       420 | 2282 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|       420 | 2283 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|       101 | 2284 | `						bBytes = 1;` |
|       370 | 2285 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|        36 | 2286 | `						return iOf1;` |
|       294 | 2287 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|        19 | 2288 | `						return -iOf2;` |
|         - | 2289 | `					}` |
|         - | 2290 | `				}` |
|       376 | 2291 | `				if( !bBytes ){` |
|         - | 2292 | `					/* Perform a numeric comparison */` |
|       276 | 2293 | `					goto Numeric;` |
|         - | 2294 | `				}` |
|        50 | 2295 | `			}` |
|    178970 | 2296 | `		}` |
|         - | 2297 | `		/* Perform a strict string comparison.*/` |
|   4153809 | 2298 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|        41 | 2299 | `			PH7_MemObjToString(pObj1);` |
|        20 | 2300 | `		}` |
|   4153809 | 2301 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|        59 | 2302 | `			PH7_MemObjToString(pObj2);` |
|        29 | 2303 | `		}` |
|   4153809 | 2304 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|   4153809 | 2305 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|         - | 2306 | `		/*` |
|         - | 2307 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|         - | 2308 | `		 * other, then the shorter value is less than the longer value.` |
|         - | 2309 | `		 */` |
|   4153809 | 2310 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|   4153809 | 2311 | `		if( rc == 0 ){` |
|   2758445 | 2312 | `			if( s1.nByte != s2.nByte ){` |
|     37542 | 2313 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|     18767 | 2314 | `			}` |
|   1379199 | 2315 | `		}` |
|   4153809 | 2316 | `		return rc;` |
|  34265378 | 2317 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|  17130088 | 2318 | `Numeric:` |
|         - | 2319 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|  34265650 | 2320 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       226 | 2321 | `			PH7_MemObjToNumeric(pObj1);` |
|       109 | 2322 | `		}` |
|  34265650 | 2323 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|       234 | 2324 | `			PH7_MemObjToNumeric(pObj2);` |
|       113 | 2325 | `		}` |
|  34265650 | 2326 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|         - | 2327 | `			/*` |
|         - | 2328 | `			 * Symisc eXtension to the PHP language:` |
|         - | 2329 | `			 *  Floating point comparison is introduced and works as expected.` |
|         - | 2330 | `			 */` |
|         - | 2331 | `			ph7_real r1,r2;` |
|         - | 2332 | `			/* Compare as reals */` |
|       898 | 2333 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        43 | 2334 | `				PH7_MemObjToReal(pObj1);` |
|        21 | 2335 | `			}` |
|       898 | 2336 | `			r1 = pObj1->rVal;` |
|       898 | 2337 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        29 | 2338 | `				PH7_MemObjToReal(pObj2);` |
|        14 | 2339 | `			}` |
|       898 | 2340 | `			r2 = pObj2->rVal;` |
|       898 | 2341 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|         - | 2342 | `				/*` |
|         - | 2343 | `				 * php's answer for an unordered pair, from either side: 1. The` |
|         - | 2344 | `				 * branch above catches every NaN that arrives AS a float; this one` |
|         - | 2345 | `				 * is for a NaN that only appears once both operands have been` |
|         - | 2346 | `				 * converted, and it must agree with it -- an antisymmetric answer` |
|         - | 2347 | ``				 * here (the old `NaN equals NaN, and is greater than everything`` |
|         - | 2348 | ``				 * else`) is what made `NAN === NAN` true and `1.5 > NAN` disagree`` |
|         - | 2349 | ``				 * with `NAN < 1.5`.`` |
|         - | 2350 | `				 */` |
|       ! 0 | 2351 | `				return 1;` |
|         - | 2352 | `			}` |
|       898 | 2353 | `			if( r1 > r2 ){` |
|        51 | 2354 | `				return 1;` |
|       850 | 2355 | `			}else if( r1 < r2 ){` |
|       226 | 2356 | `				return -1;` |
|         - | 2357 | `			}` |
|       627 | 2358 | `			return 0;` |
|       ! 0 | 2359 | `		}else{` |
|         - | 2360 | `			/* Integer comparison */` |
|  34264756 | 2361 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|   6891887 | 2362 | `				return 1;` |
|  27372874 | 2363 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|  24767562 | 2364 | `				return -1;` |
|         - | 2365 | `			}` |
|   2605317 | 2366 | `			return 0;` |
|         - | 2367 | `		}` |
|         - | 2368 | `	}` |
|         - | 2369 | `	/* NOT REACHED */` |
|       ! 0 | 2370 | `	return 0;` |
|  19555016 | 2371 | `}` |
|         - | 2372 | `/*` |
|         - | 2373 | ` * Perform an addition operation of two ph7_values.` |
|         - | 2374 | ` * The reason this function is implemented here rather than 'vm.c'` |
|         - | 2375 | ` * is that the '+' operator is overloaded.` |
|         - | 2376 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|         - | 2377 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|         - | 2378 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|         - | 2379 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|         - | 2380 | ` * will be used, and the matching elements from the right-hand array will` |
|         - | 2381 | ` * be ignored.` |
|         - | 2382 | ` * This function take care of handling all the scenarios.` |
|         - | 2383 | ` */` |
|   7200959 | 2384 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|         5 | 2385 | `{` |
|   7200964 | 2386 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2387 | `			/* Arithemtic operation */` |
|   7194165 | 2388 | `			PH7_MemObjToNumeric(pObj1);` |
|   7194165 | 2389 | `			PH7_MemObjToNumeric(pObj2);` |
|   7194165 | 2390 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|         - | 2391 | `				/* Floating point arithmetic */` |
|         - | 2392 | `				ph7_real a,b;` |
|       149 | 2393 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|        46 | 2394 | `					PH7_MemObjToReal(pObj1);` |
|        22 | 2395 | `				}` |
|       149 | 2396 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|        57 | 2397 | `					PH7_MemObjToReal(pObj2);` |
|        28 | 2398 | `				}` |
|       149 | 2399 | `				a = pObj1->rVal;` |
|       149 | 2400 | `				b = pObj2->rVal;` |
|       149 | 2401 | `				pObj1->rVal = a+b;` |
|       149 | 2402 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2403 | `				/* Try to get an integer representation also */` |
|       149 | 2404 | `				MemObjTryIntger(&(*pObj1));` |
|        76 | 2405 | `			}else{` |
|         - | 2406 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|         - | 2407 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|         - | 2408 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|         - | 2409 | `				sxi64 a,b,r;` |
|   7194019 | 2410 | `				a = pObj1->x.iVal;` |
|   7194019 | 2411 | `				b = pObj2->x.iVal;` |
|   7194019 | 2412 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|         - | 2413 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        63 | 2414 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|        63 | 2415 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|         - | 2416 | `#else` |
|         - | 2417 | `					pObj1->x.iVal = r;` |
|         - | 2418 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2419 | `#endif` |
|        32 | 2420 | `				}else{` |
|   7193957 | 2421 | `					pObj1->x.iVal = r;` |
|   7193957 | 2422 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|         - | 2423 | `				}` |
|         - | 2424 | `			}` |
|   3597165 | 2425 | `	}else{` |
|      6804 | 2426 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|         - | 2427 | `			ph7_hashmap *pMap;` |
|         - | 2428 | `			sxi32 rc;` |
|      6804 | 2429 | `			if( bAddStore ){` |
|         - | 2430 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|         - | 2431 | `				 */` |
|        53 | 2432 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 2433 | `					/* Force a hashmap cast */` |
|       ! 0 | 2434 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|       ! 0 | 2435 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 2436 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2437 | `						return rc;` |
|         - | 2438 | `					}` |
|       ! 0 | 2439 | `				}` |
|         - | 2440 | `				/* COW separate before in-place mutation */` |
|        53 | 2441 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        27 | 2442 | `			}else{` |
|         - | 2443 | `				/* Create a new hashmap */` |
|      6752 | 2444 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|      6752 | 2445 | `				if( pMap == 0){` |
|       ! 0 | 2446 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|       ! 0 | 2447 | `					return SXERR_MEM;` |
|         - | 2448 | `				}` |
|         - | 2449 | `			}` |
|      6804 | 2450 | `			if( !bAddStore ){` |
|      6752 | 2451 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|         - | 2452 | `					/* Perform a hashmap duplication */` |
|      6752 | 2453 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|      3373 | 2454 | `				}else{` |
|       ! 0 | 2455 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2456 | `						/* Simple insertion */` |
|       ! 0 | 2457 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|       ! 0 | 2458 | `					}` |
|         - | 2459 | `				}` |
|      3368 | 2460 | `			}` |
|         - | 2461 | `			/* Perform the union */` |
|      6804 | 2462 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|      6804 | 2463 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|      3399 | 2464 | `			}else{` |
|       ! 0 | 2465 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|         - | 2466 | `					/* Simple insertion */` |
|       ! 0 | 2467 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|       ! 0 | 2468 | `				}` |
|         - | 2469 | `			}` |
|         - | 2470 | `			/* Reflect the change */` |
|      6804 | 2471 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 2472 | `				SyBlobRelease(&pObj1->sBlob);` |
|       ! 0 | 2473 | `			}` |
|      6804 | 2474 | `			pObj1->x.pOther = pMap;` |
|      6804 | 2475 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|      3394 | 2476 | `		}` |
|         - | 2477 | `	}` |
|   7200964 | 2478 | `	return SXRET_OK;` |
|   3600559 | 2479 | `}` |
|         - | 2480 | `/*` |
|         - | 2481 | ` * Return a printable representation of the type of a given` |
|         - | 2482 | ` * ph7_value.` |
|         - | 2483 | ` */` |
|        10 | 2484 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|         3 | 2485 | `{` |
|        13 | 2486 | `	const char *zType = "";` |
|        13 | 2487 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|       ! 0 | 2488 | `		zType = "null";` |
|        13 | 2489 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|         - | 2490 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|         - | 2491 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|       ! 0 | 2492 | `		zType = "double";` |
|        13 | 2493 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|       ! 0 | 2494 | `		zType = "int";` |
|        13 | 2495 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|         8 | 2496 | `		zType = "string";` |
|         9 | 2497 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|       ! 0 | 2498 | `		zType = "bool";` |
|         6 | 2499 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|         6 | 2500 | `		zType = "array";` |
|         2 | 2501 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 2502 | `		zType = "object";` |
|       ! 0 | 2503 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 2504 | `		zType = "resource";` |
|       ! 0 | 2505 | `	}` |
|        13 | 2506 | `	return zType;` |
|         3 | 2507 | `}` |
|         - | 2508 | `/*` |
|         - | 2509 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|         - | 2510 | ` * Store the dump in the given blob.` |
|         - | 2511 | ` */` |
|         - | 2512 | `/*` |
|         - | 2513 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|         - | 2514 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|         - | 2515 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|         - | 2516 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|         - | 2517 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|         - | 2518 | ` */` |
|       400 | 2519 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|         5 | 2520 | `{` |
|         - | 2521 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - | 2522 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|         - | 2523 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|         - | 2524 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|         - | 2525 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|         - | 2526 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|         - | 2527 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|         - | 2528 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|         - | 2529 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|       405 | 2530 | `	PH7_AppendShortestReal(pOut,rVal);` |
|         - | 2531 | `#else` |
|         - | 2532 | `	if( PH7_IS_NAN(rVal) ){` |
|         - | 2533 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|         - | 2534 | `	}else if( PH7_IS_INF(rVal) ){` |
|         - | 2535 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|         - | 2536 | `	}else{` |
|         - | 2537 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|         - | 2538 | `	}` |
|         - | 2539 | `#endif` |
|       405 | 2540 | `}` |
|         - | 2541 | `/*` |
|         - | 2542 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|         - | 2543 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|         - | 2544 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|         - | 2545 | ` */` |
|      1256 | 2546 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|         5 | 2547 | `{` |
|         - | 2548 | `	/* print_r RENDERS through the string coercion -- unlike var_dump and` |
|         - | 2549 | `	 * var_export, which describe the value instead -- so php's NaN warning` |
|         - | 2550 | `	 * belongs here too, once per value it prints. */` |
|      1256 | 2551 | `	if( (pObj->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|       636 | 2552 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|         3 | 2553 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|         - | 2554 | `			"unexpected NAN value was coerced to string");` |
|         1 | 2555 | `	}` |
|      1261 | 2556 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|       107 | 2557 | `		return;` |
|         - | 2558 | `	}` |
|      1155 | 2559 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|        70 | 2560 | `		if( pObj->x.iVal != 0 ){` |
|        21 | 2561 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|        10 | 2562 | `		}` |
|        70 | 2563 | `		return;` |
|         - | 2564 | `	}` |
|      1087 | 2565 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|         - | 2566 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|         - | 2567 | `		 * non-strings into the output) */` |
|       647 | 2568 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       631 | 2569 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|       313 | 2570 | `		}` |
|       647 | 2571 | `		return;` |
|         - | 2572 | `	}` |
|       444 | 2573 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       633 | 2574 | `}` |
|         - | 2575 | `/*` |
|         - | 2576 | ` * php's GC_PROTECT_RECURSION, read: TRUE when this value is a container the dump` |
|         - | 2577 | ` * currently walking is already INSIDE. php carries one protection bit per` |
|         - | 2578 | ` * array/object and every dumper -- var_dump, print_r, var_export -- consults the` |
|         - | 2579 | ` * same one; PHL carries HASHMAP_DUMPING and VM_INSTANCE_DUMPING for that, so this` |
|         - | 2580 | ` * is the single place that reads them.` |
|         - | 2581 | ` *` |
|         - | 2582 | ` * It is an ANCESTOR test, not a "seen before" test: an object reached twice down` |
|         - | 2583 | ` * two sibling branches renders in full both times, and only a container that` |
|         - | 2584 | ` * contains itself is replaced by the marker.` |
|         - | 2585 | ` */` |
|     28446 | 2586 | `PH7_PRIVATE int PH7_MemObjDumpIsRecursive(ph7_value *pObj)` |
|         5 | 2587 | `{` |
|     28451 | 2588 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      2529 | 2589 | `		return (((ph7_hashmap *)pObj->x.pOther)->iFlags & HASHMAP_DUMPING) != 0;` |
|         - | 2590 | `	}` |
|     25927 | 2591 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       473 | 2592 | `		return (((ph7_class_instance *)pObj->x.pOther)->iFlags & VM_INSTANCE_DUMPING) != 0;` |
|         - | 2593 | `	}` |
|     25459 | 2594 | `	return 0;` |
|     14152 | 2595 | `}` |
|     22612 | 2596 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|         - | 2597 | `	SyBlob *pOut,      /* Store the dump here */` |
|         - | 2598 | `	ph7_value *pObj,   /* Dump this */` |
|         - | 2599 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|         - | 2600 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|         - | 2601 | `	                    * print_r = the container's parenthesis column */` |
|         - | 2602 | `	int nDepth,        /* Nesting level */` |
|         - | 2603 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|         - | 2604 | `	)` |
|         5 | 2605 | `{` |
|     22617 | 2606 | `	sxi32 rc = SXRET_OK;` |
|         - | 2607 | `	int i;` |
|     22617 | 2608 | `	if( !ShowType ){` |
|         - | 2609 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|         - | 2610 | `		 * containers render the Array/Object block (which the container` |
|         - | 2611 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|       501 | 2612 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       339 | 2613 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2614 | `		}` |
|       167 | 2615 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       163 | 2616 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|         - | 2617 | `		}` |
|         5 | 2618 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|         5 | 2619 | `		return SXRET_OK;` |
|         - | 2620 | `	}` |
|         - | 2621 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|         - | 2622 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|         - | 2623 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|     37517 | 2624 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     15401 | 2625 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      7703 | 2626 | `	}` |
|     22121 | 2627 | `	if( PH7_MemObjDumpIsRecursive(pObj) ){` |
|         - | 2628 | `		/* php replaces the WHOLE value -- header, body and the '&' a referenced` |
|         - | 2629 | `		 * entry would otherwise carry -- with the marker, and never descends. This` |
|         - | 2630 | `		 * is also why PH7_HashmapDump/PH7_ClassInstanceDump never see a marked` |
|         - | 2631 | `		 * container in var_dump mode: they are reached only from here. */` |
|        12 | 2632 | `		SyBlobAppend(&(*pOut),"*RECURSION*\n",sizeof("*RECURSION*\n")-1);` |
|        12 | 2633 | `		return SXRET_OK;` |
|         - | 2634 | `	}` |
|     22111 | 2635 | `	if( isRef ){` |
|       122 | 2636 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|        59 | 2637 | `	}` |
|     22111 | 2638 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|       405 | 2639 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       405 | 2640 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|         - | 2641 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|        20 | 2642 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|        20 | 2643 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sDisp);` |
|        20 | 2644 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        20 | 2645 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|         9 | 2646 | `			}` |
|        20 | 2647 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        20 | 2648 | `			return SXRET_OK;` |
|         - | 2649 | `		}` |
|       387 | 2650 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|       387 | 2651 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       387 | 2652 | `		return rc;` |
|         - | 2653 | `	}` |
|     21711 | 2654 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|      1022 | 2655 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|      1022 | 2656 | `		return SXRET_OK;` |
|         - | 2657 | `	}` |
|     20694 | 2658 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      1894 | 2659 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|      1894 | 2660 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      1894 | 2661 | `		return rc;` |
|         - | 2662 | `	}` |
|     18805 | 2663 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      6391 | 2664 | `		if( pObj->x.iVal != 0 ){` |
|      3609 | 2665 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|      1799 | 2666 | `		}else{` |
|      2787 | 2667 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|         - | 2668 | `		}` |
|      6391 | 2669 | `		return SXRET_OK;` |
|         - | 2670 | `	}` |
|     12419 | 2671 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         - | 2672 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|         - | 2673 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|       405 | 2674 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|       405 | 2675 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|       405 | 2676 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|       405 | 2677 | `		return SXRET_OK;` |
|         - | 2678 | `	}` |
|     12019 | 2679 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|      5152 | 2680 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|      5152 | 2681 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      5152 | 2682 | `		return SXRET_OK;` |
|         - | 2683 | `	}` |
|      6872 | 2684 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|      6872 | 2685 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|      6872 | 2686 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      6460 | 2687 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      3223 | 2688 | `		}` |
|      6872 | 2689 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|      6872 | 2690 | `		return SXRET_OK;` |
|         - | 2691 | `	}` |
|       ! 0 | 2692 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|         - | 2693 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|         - | 2694 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|         - | 2695 | `		 * shape printed the heap pointer through the string cast instead. */` |
|       ! 0 | 2696 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|       ! 0 | 2697 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|       ! 0 | 2698 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|       ! 0 | 2699 | `		return SXRET_OK;` |
|         - | 2700 | `	}` |
|         - | 2701 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|         - | 2702 | `	{` |
|       ! 0 | 2703 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|       ! 0 | 2704 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|       ! 0 | 2705 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|       ! 0 | 2706 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|       ! 0 | 2707 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|         - | 2708 | `	}` |
|       ! 0 | 2709 | `	return rc;` |
|     11290 | 2710 | `}` |
|         - | 2711 |  |
