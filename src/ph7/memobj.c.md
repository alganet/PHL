# src/ph7/memobj.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1125/1270 lines (88.58%)

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
|        - |   12 | `#if defined(PHL_VALUE_CENSUS)` |
|        - |   13 | `/*` |
|        - |   14 | ` * PHL_VALUE_CENSUS -- which CALL SITE spends the engine's value primitives.` |
|        - |   15 | ` * ---------------------------------------------------------------------------` |
|        - |   16 | ` * Compiled out entirely unless PHL_VALUE_CENSUS is defined; see PERF.md §7 and` |
|        - |   17 | ` * build-aux/valuecensus.sh, which builds it and resolves what it prints.` |
|        - |   18 | ` *` |
|        - |   19 | ` * The fourth instrument, and it exists because the other three cannot see this.` |
|        - |   20 | ` * The heap census answers "where are the bytes"; the lookup census answers "which` |
|        - |   21 | ` * line hashes the names". PH7_MemObjRelease allocates nothing and looks nothing` |
|        - |   22 | ` * up, so it appears in NEITHER -- and it is the most-called function in the` |
|        - |   23 | ` * engine: 2.72 billion calls on the ecosystem gate's phpcs step, 43.1% of them on` |
|        - |   24 | ` * a value that owned nothing (PERF.md §2, P10). A whole-program counter said that` |
|        - |   25 | ` * much; it could not say WHICH of the ~700 call sites made those calls, which is` |
|        - |   26 | ` * the question a design has to answer.` |
|        - |   27 | ` *` |
|        - |   28 | ` * One record per (return address, primitive), so a site is a place in the SOURCE` |
|        - |   29 | ` * and not a function: the release inside VmOperandStackRecycle's loop and the one` |
|        - |   30 | ` * in VmPopOperand are two rows, which is what a change is aimed at. Addresses are` |
|        - |   31 | ` * emitted relative to the PIE load base (the ADDRESS of __executable_start is that` |
|        - |   32 | ` * base at run time), so addr2line takes them exactly as printed -- the other two` |
|        - |   33 | ` * censuses' convention, for the same reason.` |
|        - |   34 | ` *` |
|        - |   35 | `` * `nWork` is the half the call-count alone cannot give: how many of a site's calls`` |
|        - |   36 | ` * had anything to DO. For a release that is the slow path (it owned a string, a` |
|        - |   37 | ` * container reference or an AUX carrier); for a load/store it is a container` |
|        - |   38 | ` * reference taken; for an init it is always 1. A site with a large count and a` |
|        - |   39 | ` * near-zero nWork is the engine building and tearing down slots that never held` |
|        - |   40 | ` * anything -- which is P10 item 1, and it is what this instrument was built to` |
|        - |   41 | ` * find.` |
|        - |   42 | ` *` |
|        - |   43 | ` * The table is fixed-size and static, for the other censuses' reason: it must not` |
|        - |   44 | ` * allocate through the allocator whose values it is counting. Nothing is ever` |
|        - |   45 | ` * deleted (a site is a code address), so a full table refuses to record and says` |
|        - |   46 | ` * TRUNCATED rather than under-count.` |
|        - |   47 | ` */` |
|        - |   48 | `#include <stdio.h>` |
|        - |   49 | `#include <stdlib.h>` |
|        - |   50 |  |
|        - |   51 | `extern char __executable_start[];   /* its ADDRESS is the PIE load base */` |
|        - |   52 |  |
|        - |   53 | `typedef struct phl_vcensus_rec phl_vcensus_rec;` |
|        - |   54 | `struct phl_vcensus_rec {` |
|        - |   55 | `	void *pSite;     /* PHL_VCENSUS_SITE() at the door; 0 = free slot */` |
|        - |   56 | `	sxu32 iKind;     /* PHL_VC_* -- which primitive was called */` |
|        - |   57 | `	sxu64 nCall;     /* calls made from here */` |
|        - |   58 | `	sxu64 nWork;     /* how many of them had anything to do */` |
|        - |   59 | `};` |
|        - |   60 | `#define PHL_VCENSUS_SLOTS 8192` |
|        - |   61 | `static struct {` |
|        - |   62 | `	int bReady;      /* 0 = untouched, 1 = live */` |
|        - |   63 | `	int bFull;       /* the table filled; recording stopped */` |
|        - |   64 | `	phl_vcensus_rec aRec[PHL_VCENSUS_SLOTS];` |
|        - |   65 | `	sxu64 aCall[PHL_VC_KINDS],aWork[PHL_VC_KINDS];` |
|        - |   66 | `} sVCensus;` |
|        - |   67 |  |
|        - |   68 | `static void VCensusDump(void)` |
|        - |   69 | `{` |
|        - |   70 | `	const char *zOut = getenv("PHL_VCENSUS_OUT");` |
|        - |   71 | `	FILE *pOut = zOut ? fopen(zOut,"w") : stderr;` |
|        - |   72 | `	sxu32 i;` |
|        - |   73 | `	if( pOut == 0 ){` |
|        - |   74 | `		pOut = stderr;` |
|        - |   75 | `	}` |
|        - |   76 | `	for( i = 0 ; i < PHL_VC_KINDS ; ++i ){` |
|        - |   77 | `		fprintf(pOut,"# kind %u %llu %llu%s\n",i,` |
|        - |   78 | `			(unsigned long long)sVCensus.aCall[i],(unsigned long long)sVCensus.aWork[i],` |
|        - |   79 | `			sVCensus.bFull ? "  TRUNCATED" : "");` |
|        - |   80 | `	}` |
|        - |   81 | `	for( i = 0 ; i < PHL_VCENSUS_SLOTS ; ++i ){` |
|        - |   82 | `		phl_vcensus_rec *pRec = &sVCensus.aRec[i];` |
|        - |   83 | `		if( pRec->pSite == 0 ){` |
|        - |   84 | `			continue;` |
|        - |   85 | `		}` |
|        - |   86 | `		fprintf(pOut,"SITE 0x%lx %u %llu %llu\n",` |
|        - |   87 | `			(unsigned long)((char *)pRec->pSite - __executable_start),` |
|        - |   88 | `			pRec->iKind,` |
|        - |   89 | `			(unsigned long long)pRec->nCall,(unsigned long long)pRec->nWork);` |
|        - |   90 | `	}` |
|        - |   91 | `	if( pOut != stderr ){` |
|        - |   92 | `		fclose(pOut);` |
|        - |   93 | `	}` |
|        - |   94 | `}` |
|        - |   95 | `PH7_PRIVATE void PH7_ValueCensusNote(void *pSite,sxu32 iKind,int bWork)` |
|        - |   96 | `{` |
|        - |   97 | `	/* Fibonacci scramble: the low bits of a code address are not a key, and the` |
|        - |   98 | `	 * kind has to be in it or one line's release and load share a slot. */` |
|        - |   99 | `	sxu64 x = (sxu64)(sxuptr)pSite ^ ((sxu64)iKind * (sxu64)0x9e3779b97f4a7c15ULL);` |
|        - |  100 | `	sxu32 i,n;` |
|        - |  101 | `	if( !sVCensus.bReady ){` |
|        - |  102 | `		sVCensus.bReady = 1;` |
|        - |  103 | `		atexit(VCensusDump);` |
|        - |  104 | `	}` |
|        - |  105 | `	x ^= x >> 33; x *= (sxu64)0xff51afd7ed558ccdULL; x ^= x >> 29;` |
|        - |  106 | `	i = (sxu32)x & (PHL_VCENSUS_SLOTS - 1);` |
|        - |  107 | `	for( n = 0 ; n < PHL_VCENSUS_SLOTS ; ++n ){` |
|        - |  108 | `		phl_vcensus_rec *pRec = &sVCensus.aRec[i];` |
|        - |  109 | `		if( pRec->pSite == 0 ){` |
|        - |  110 | `			pRec->pSite = pSite;` |
|        - |  111 | `			pRec->iKind = iKind;` |
|        - |  112 | `		}` |
|        - |  113 | `		if( pRec->pSite == pSite && pRec->iKind == iKind ){` |
|        - |  114 | `			pRec->nCall++;` |
|        - |  115 | `			pRec->nWork += bWork ? 1 : 0;` |
|        - |  116 | `			sVCensus.aCall[iKind]++;` |
|        - |  117 | `			sVCensus.aWork[iKind] += bWork ? 1 : 0;` |
|        - |  118 | `			return;` |
|        - |  119 | `		}` |
|        - |  120 | `		i = (i + 1) & (PHL_VCENSUS_SLOTS - 1);` |
|        - |  121 | `	}` |
|        - |  122 | `	sVCensus.bFull = 1;   /* said out loud in the dump rather than counted wrong */` |
|        - |  123 | `}` |
|        - |  124 | `#endif /* PHL_VALUE_CENSUS */` |
|        - |  125 |  |
|        - |  126 | `/* Portable 64-bit overflow-detecting arithmetic for compilers that lack the` |
|        - |  127 | ` * GCC/Clang __builtin_*_overflow intrinsics (i.e. MSVC). The header exposes` |
|        - |  128 | ` * these through the PH7_{ADD,SUB,MUL}_OVERFLOW64 macros; the intrinsic path` |
|        - |  129 | ` * needs no out-of-line definition, so gate the whole block off there to avoid` |
|        - |  130 | ` * an unused-function warning. Each sets *pR to the two's-complement wrapped` |
|        - |  131 | ` * result and returns non-zero on overflow. The additive checks compute the` |
|        - |  132 | ` * wrapped result via unsigned math (no signed-overflow UB) and test the sign` |
|        - |  133 | ` * bits; the multiplicative check mirrors vm.c's proven bound-check form. */` |
|        - |  134 | `#if !(defined(__GNUC__) \|\| defined(__clang__))` |
|        - |  135 | `PH7_PRIVATE int PH7_AddOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |  136 | `{` |
|        5 |  137 | `	*pR = (sxi64)((sxu64)a + (sxu64)b);` |
|        - |  138 | `	/* Overflow iff the operands share a sign and the result's sign differs. */` |
|        5 |  139 | `	return ((a ^ *pR) & (b ^ *pR)) < 0;` |
|        5 |  140 | `}` |
|        - |  141 | `PH7_PRIVATE int PH7_SubOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |  142 | `{` |
|        5 |  143 | `	*pR = (sxi64)((sxu64)a - (sxu64)b);` |
|        - |  144 | `	/* Overflow iff the operands differ in sign and the result's sign differs` |
|        - |  145 | `	 * from the minuend's. */` |
|        5 |  146 | `	return ((a ^ b) & (a ^ *pR)) < 0;` |
|        5 |  147 | `}` |
|        - |  148 | `PH7_PRIVATE int PH7_MulOverflow64(sxi64 a,sxi64 b,sxi64 *pR)` |
|        5 |  149 | `{` |
|        5 |  150 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        5 |  151 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        5 |  152 | `		return 0;` |
|        - |  153 | `	}` |
|        5 |  154 | `	if( a == -1 ){` |
|        1 |  155 | `		return b == SMALLEST_INT64;` |
|        - |  156 | `	}` |
|        5 |  157 | `	if( b == -1 ){` |
|        1 |  158 | `		return a == SMALLEST_INT64;` |
|        - |  159 | `	}` |
|        5 |  160 | `	if( a > 0 ){` |
|        5 |  161 | `		if( b > 0 ){` |
|        5 |  162 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |  163 | `		}else{` |
|        1 |  164 | `			return b < SMALLEST_INT64 / a;` |
|        - |  165 | `		}` |
|      ! 0 |  166 | `	}else{` |
|        1 |  167 | `		if( b > 0 ){` |
|        1 |  168 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |  169 | `		}else{` |
|        1 |  170 | `			return b < LARGEST_INT64 / a;` |
|        - |  171 | `		}` |
|        - |  172 | `	}` |
|        5 |  173 | `}` |
|        - |  174 | `#endif` |
|        - |  175 |  |
|        - |  176 | `/* Provide PHP-style type names for values.  This utility may be reused` |
|        - |  177 | ` * by any subsystem that works with ph7_value.` |
|        - |  178 | ` */` |
|     8288 |  179 | `PH7_PRIVATE const char *ph7_type_name(ph7_value *pVal)` |
|        5 |  180 | `{` |
|     8293 |  181 | `	if( ph7_value_is_null(pVal) ) return "null";` |
|     7836 |  182 | `	if( ph7_value_is_bool(pVal) ) return "bool";` |
|        - |  183 | `	/* FLOAT before INT: ph7_value_is_int() is deliberately lenient — an` |
|        - |  184 | `	 * integer-valued real caches an int and answers TRUE — so asking it first named` |
|        - |  185 | `	 * a float "int" in every diagnostic that quotes a value's type` |
|        - |  186 | ``	 * (`sort(1.0)` said `must be of type array, int given` where php says `float`).`` |
|        - |  187 | `	 * A value that IS a float is a float whatever it has cached. */` |
|     7298 |  188 | `	if( ph7_value_is_float(pVal) ) return "float";` |
|     7167 |  189 | `	if( ph7_value_is_int(pVal) ) return "int";` |
|     5043 |  190 | `	if( ph7_value_is_string(pVal) ) return "string";` |
|     1578 |  191 | `	if( ph7_value_is_array(pVal) ) return "array";` |
|       36 |  192 | `	if( ph7_value_is_object(pVal) ) return "object";` |
|       36 |  193 | `	if( ph7_value_is_resource(pVal) ) return "resource";` |
|      ! 0 |  194 | `	return "unknown";` |
|     4145 |  195 | `}` |
|        - |  196 |  |
|        - |  197 | `/*` |
|        - |  198 | ` * Notes on memory objects [i.e: ph7_value].` |
|        - |  199 | ` * Internally, the PH7 virtual machine manipulates nearly all PHP values` |
|        - |  200 | ` * [i.e: string,int,float,resource,object,bool,null..] as ph7_values structures.` |
|        - |  201 | ` * Each ph7_values struct may cache multiple representations (string,` |
|        - |  202 | ` * integer etc.) of the same value.` |
|        - |  203 | ` */` |
|        - |  204 | `/*` |
|        - |  205 | ` * TRUE when a double is what an int64 can hold exactly -- php's` |
|        - |  206 | ` * ZEND_DOUBLE_FITS_LONG with its non-finite screen folded in. The bounds are` |
|        - |  207 | ` * tested in DOUBLE space and the arithmetic there is exact: -2^63 is a double` |
|        - |  208 | ` * to the bit and so is +2^63, one past the range, with no double in between it` |
|        - |  209 | `` * and LARGEST_INT64. Hence `>=` on the way down and `<` on the way up. NaN and`` |
|        - |  210 | ` * both infinities fail one of the two comparisons, so no libm predicate is` |
|        - |  211 | ` * needed to screen them.` |
|        - |  212 | ` */` |
|    26210 |  213 | `PH7_PRIVATE int PH7_RealFitsInt64(double r)` |
|        5 |  214 | `{` |
|    26215 |  215 | `	return r >= -9223372036854775808.0 && r < 9223372036854775808.0;` |
|        5 |  216 | `}` |
|        - |  217 | `/*` |
|        - |  218 | ` * Convert a 64-bit IEEE double into a 64-bit signed integer -- php's` |
|        - |  219 | ` * zend_dval_to_lval, the answer every CAST site gives for a double no int can` |
|        - |  220 | ` * hold: NaN and both infinities are 0, and a finite out-of-range value WRAPS` |
|        - |  221 | `` * modulo 2^64 into the signed band (`(int)1e19` is -8446744073709551616,`` |
|        - |  222 | `` * `(int)1e30` is 5076964154930102272, `(int)1e100` is 0 because every one of`` |
|        - |  223 | ` * its low 64 bits is).` |
|        - |  224 | ` *` |
|        - |  225 | ` * PHL used to answer PHP_INT_MIN for all of them, in silence -- a recorded §2` |
|        - |  226 | ` * divergence, and a silent wrong answer wherever a program casts a computed` |
|        - |  227 | ` * float. The warning php prints beside the value is the cast SITE's to raise:` |
|        - |  228 | ` * this is also the conversion an int representation is speculatively cached` |
|        - |  229 | ` * through (MemObjTryIntger), where php says nothing at all.` |
|        - |  230 | ` *` |
|        - |  231 | ` * php reaches the wrap through fmod(d, 2^64); the same answer comes out of the` |
|        - |  232 | ` * IEEE bits with no libm. A double of magnitude >= 2^63 is already an exact` |
|        - |  233 | ` * integer -- its mantissa is scaled by 2^11 at least -- so the low 64 bits are` |
|        - |  234 | ` * the 53-bit mantissa shifted LEFT, which is 0 once the shift reaches 64.` |
|        - |  235 | ` */` |
|    25044 |  236 | `PH7_PRIVATE sxi64 PH7_RealToInt64(double r)` |
|        5 |  237 | `{` |
|        - |  238 | `  union { double d; sxu64 u; } bits;` |
|        - |  239 | `  sxu64 uMag;` |
|        - |  240 | `  int iShift;` |
|    25049 |  241 | `  if( PH7_RealFitsInt64(r) ){` |
|        - |  242 | `    /* In range: php truncates toward zero, and so does C. */` |
|    23433 |  243 | `    return (sxi64)r;` |
|        - |  244 | `  }` |
|     1621 |  245 | `  if( PH7_IS_NAN(r) \|\| PH7_IS_INF(r) ){` |
|      546 |  246 | `    return 0;` |
|        - |  247 | `  }` |
|     1079 |  248 | `  bits.d = r;` |
|        - |  249 | `  /* Unbiased exponent, minus the 52 fraction bits: the power of two the` |
|        - |  250 | `  ** mantissa is scaled by. \|r\| >= 2^63 puts it at 11 or more. */` |
|     1079 |  251 | `  iShift = (int)((bits.u >> 52) & 0x7FF) - 1023 - 52;` |
|     1079 |  252 | `  if( iShift >= 64 ){` |
|        - |  253 | `    /* Every set bit sits above the 64th, so the residue is 0 -- and the shift` |
|        - |  254 | `    ** below would be undefined. */` |
|      181 |  255 | `    return 0;` |
|        - |  256 | `  }` |
|      901 |  257 | `  uMag = ((bits.u & 0x000FFFFFFFFFFFFFULL) \| 0x0010000000000000ULL) << iShift;` |
|      901 |  258 | `  if( bits.u >> 63 ){` |
|        - |  259 | ``     /* Unsigned negation is the two's-complement residue php's `dmod += 2^64` `` |
|        - |  260 | `    ** arrives at, and is defined for every input including 0. */` |
|       51 |  261 | `    uMag = (sxu64)0 - uMag;` |
|       25 |  262 | `  }` |
|      901 |  263 | `  return (sxi64)uMag;` |
|    12507 |  264 | `}` |
|    24536 |  265 | `static sxi64 MemObjRealToInt(ph7_value *pObj)` |
|        5 |  266 | `{` |
|        - |  267 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  268 | `	/* Real and 64bit integer are the same when floating point arithmetic` |
|        - |  269 | `	 * is omitted from the build.` |
|        - |  270 | `	 */` |
|        - |  271 | `	return pObj->rVal;` |
|        - |  272 | `#else` |
|    24541 |  273 | `	return PH7_RealToInt64(pObj->rVal);` |
|        - |  274 | `#endif` |
|        5 |  275 | `}` |
|        - |  276 | `/*` |
|        - |  277 | `` * php's `Warning: The float %s is not representable as an int, cast occurred`,`` |
|        - |  278 | ` * printed BESIDE the wrapped value MemObjRealToInt answers -- at every CAST` |
|        - |  279 | `` * site, which is what php's zend_dval_to_lval raises it from: `(int)$f`,`` |
|        - |  280 | `` * `intval()`, `settype()`, the printf integer conversions, and a native`` |
|        - |  281 | ` * subscript that reads an int out of its offset.` |
|        - |  282 | ` *` |
|        - |  283 | `` * Not a DEPRECATION: php's other float->int diagnostic (`Implicit conversion`` |
|        - |  284 | `` * from float %s to int loses precision`) is the E_DEPRECATED that §10 refuses`` |
|        - |  285 | ` * outright with a TypeError, and it fires at the sites this one does NOT --` |
|        - |  286 | ` * the operators, the array key, the int parameter, none of which reach a cast` |
|        - |  287 | ` * here because the refusal comes first. An explicit cast is never lossy in` |
|        - |  288 | ` * php's eyes, so this warning is all it says. The two other conversions that` |
|        - |  289 | ` * read an int out of a float say nothing at all and must not call this: the` |
|        - |  290 | ` * speculative int representation (MemObjTryIntger) and php's string-offset` |
|        - |  291 | ` * cast, which has a message of its own.` |
|        - |  292 | ` *` |
|        - |  293 | `` * The value is rendered the way php's `%.*H` renders it -- the shortest`` |
|        - |  294 | ` * decimal that round-trips, the shape var_dump and serialize already share.` |
|        - |  295 | ` */` |
|      830 |  296 | `PH7_PRIVATE void PH7_RealWarnIntCast(ph7_vm *pVm,double r)` |
|        3 |  297 | `{` |
|        - |  298 | `	SyBlob sVal;` |
|        - |  299 | `	char zVal[64];` |
|      833 |  300 | `	if( pVm == 0 \|\| PH7_RealFitsInt64(r) ){` |
|      617 |  301 | `		return;` |
|        - |  302 | `	}` |
|      219 |  303 | `	SyBlobInitFromBuf(&sVal,zVal,(sxu32)sizeof(zVal) - 1);` |
|      219 |  304 | `	PH7_AppendShortestReal(&sVal,r);` |
|      219 |  305 | `	zVal[SyBlobLength(&sVal)] = 0;   /* the blob is LOCKED: it truncates, never grows */` |
|      327 |  306 | `	VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|      108 |  307 | `		"The float %s is not representable as an int, cast occurred",zVal);` |
|      418 |  308 | `}` |
|        - |  309 | `/* The same warning asked of a VALUE: only a float can carry one, and the flag` |
|        - |  310 | ` * test mirrors the conversion's own routing (MemObjIntValue reads MEMOBJ_REAL` |
|        - |  311 | ` * first), so the diagnostic and the answer always describe the same branch. */` |
|  1182144 |  312 | `PH7_PRIVATE void PH7_MemObjWarnIntCast(ph7_value *pObj)` |
|        5 |  313 | `{` |
|  1182149 |  314 | `	if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|  1181809 |  315 | `		return;` |
|        - |  316 | `	}` |
|      343 |  317 | `	PH7_RealWarnIntCast(pObj->pVm,(double)pObj->rVal);` |
|   591020 |  318 | `}` |
|        - |  319 | `/*` |
|        - |  320 | ` * Convert a raw token value typically a stream of digit [i.e: hex,octal,binary or decimal]` |
|        - |  321 | ` * to a 64-bit integer.` |
|        - |  322 | ` */` |
|   756719 |  323 | `PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pVal)` |
|        5 |  324 | `{` |
|   756724 |  325 | `	sxi64 iVal = 0;` |
|   756724 |  326 | `	if( pVal->nByte <= 0 ){` |
|      ! 0 |  327 | `		return 0;` |
|        - |  328 | `	}` |
|   756724 |  329 | `	if( pVal->zString[0] == '0' ){` |
|        - |  330 | `		sxi32 c;` |
|   284488 |  331 | `		if( pVal->nByte == sizeof(char) ){` |
|   276959 |  332 | `			return 0;` |
|        - |  333 | `		}` |
|     7534 |  334 | `		c = pVal->zString[1];` |
|     7534 |  335 | `		if( c  == 'x' \|\| c == 'X' ){` |
|        - |  336 | `			/* Hex digit stream */` |
|      319 |  337 | `			SyHexStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     7376 |  338 | `		}else if( c == 'b' \|\| c == 'B' ){` |
|        - |  339 | `			/* Binary digit stream */` |
|      285 |  340 | `			SyBinaryStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|     7077 |  341 | `		}else if( c == 'o' \|\| c == 'O' ){` |
|        - |  342 | `			/* PHP 8.1 explicit octal 0o/0O: skip the two-char prefix and parse the` |
|        - |  343 | `			 * remaining octal digits (SyOctalStrToInt64 expects no letter prefix). */` |
|       21 |  344 | `			if( pVal->nByte > 2 ){` |
|       21 |  345 | `				SyOctalStrToInt64(pVal->zString + 2,pVal->nByte - 2,(void *)&iVal,0);` |
|       10 |  346 | `			}` |
|       11 |  347 | `		}else{` |
|        - |  348 | `			/* Legacy octal digit stream (leading 0) */` |
|     6915 |  349 | `			SyOctalStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  350 | `		}` |
|     3763 |  351 | `	}else{` |
|        - |  352 | `		/* Decimal digit stream */` |
|   472241 |  353 | `		SyStrToInt64(pVal->zString,pVal->nByte,(void *)&iVal,0);` |
|        - |  354 | `	}` |
|   479770 |  355 | `	return iVal;` |
|   377829 |  356 | `}` |
|        - |  357 | `/*` |
|        - |  358 | ` * TRUE when the numeric PREFIX that ends at zTail is float-SHAPED -- it carries` |
|        - |  359 | ` * a '.' or a complete exponent. This is php's is_numeric_string answering` |
|        - |  360 | ` * IS_DOUBLE, and it decides which of two entirely different readings the bytes` |
|        - |  361 | ` * get: an integer-shaped run is read from its DIGITS, a float-shaped one from` |
|        - |  362 | ` * the double they spell.` |
|        - |  363 | ` */` |
|  1178317 |  364 | `static int MemObjNumericPrefixIsFloat(ph7_value *pObj,const char *zTail)` |
|        5 |  365 | `{` |
|  1178322 |  366 | `	const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|  3931937 |  367 | `	while( z < zTail ){` |
|  2753910 |  368 | `		if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|      294 |  369 | `			return TRUE;` |
|        - |  370 | `		}` |
|  2753620 |  371 | `		z++;` |
|        5 |  372 | `	}` |
|  1178032 |  373 | `	return FALSE;` |
|   589151 |  374 | `}` |
|        - |  375 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  376 | `/*` |
|        - |  377 | ` * php's zend_dval_to_lval_cap: the double->int conversion a NUMERIC STRING` |
|        - |  378 | ` * takes, which is not the one a real float takes. This one SATURATES at the` |
|        - |  379 | ` * int64 bounds and answers 0 for a value that is not finite, where the cast of` |
|        - |  380 | ` * an actual float answers PHP_INT_MIN for every out-of-range case` |
|        - |  381 | ` * (MemObjRealToInt -- a recorded divergence). PHL has always` |
|        - |  382 | `` * saturated the integer-shaped overflow, so `(int)"99999999999999999999"` is`` |
|        - |  383 | ` * PHP_INT_MAX in both engines; this is the same rule for the shapes that reach` |
|        - |  384 | ` * it through a double.` |
|        - |  385 | ` */` |
|      210 |  386 | `static sxi64 MemObjRealToIntCap(ph7_real r)` |
|        4 |  387 | `{` |
|        - |  388 | `	/* NaN fails both comparisons and either infinity fails one of them, so this` |
|        - |  389 | `	 * screens all three without a libm predicate. */` |
|      214 |  390 | `	if( !(r >= -1.7976931348623157e308 && r <= 1.7976931348623157e308) ){` |
|       16 |  391 | `		return 0;` |
|        - |  392 | `	}` |
|      200 |  393 | `	if( r >= 9223372036854775808.0 ){    /* +2^63, exact in double space */` |
|       24 |  394 | `		return LARGEST_INT64;` |
|        - |  395 | `	}` |
|      177 |  396 | `	if( r < -9223372036854775808.0 ){` |
|        5 |  397 | `		return SMALLEST_INT64;` |
|        - |  398 | `	}` |
|      173 |  399 | `	return (sxi64)r;` |
|      109 |  400 | `}` |
|        - |  401 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  402 | `/*` |
|        - |  403 | ` * Return some kind of 64-bit integer value which is the best we can` |
|        - |  404 | ` * do at representing the value that pObj describes as a string` |
|        - |  405 | ` * representation.` |
|        - |  406 | ` */` |
|  1177585 |  407 | `static sxi64 MemObjStringToInt(ph7_value *pObj,int *pOverflow)` |
|        5 |  408 | `{` |
|  1177590 |  409 | `	sxi64 iVal = 0;` |
|        - |  410 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|  1177590 |  411 | `	const char *zTail = 0;` |
|  1177585 |  412 | `	if( PH7_MemObjStringNumericPrefix(pObj,&zTail)` |
|  1177579 |  413 | `	 && MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|        - |  414 | `		/* A float-shaped string is a DOUBLE first and an int second, which is the` |
|        - |  415 | ``		 * only reading that makes `(int)"1e3"` the 1000 it says: reading its`` |
|        - |  416 | `		 * digits stops at the 'e' and answers the mantissa's integer part, so` |
|        - |  417 | `		 * "1e3" was 1, "1.5e2" was 1 and "-2e2" was -2. The '.' forms were wrong` |
|        - |  418 | `		 * the same way wherever the double rounds away from the digits --` |
|        - |  419 | ``		 * `(int)"0.9999999999999999999"` is 1, not 0. */`` |
|      214 |  420 | `		ph7_real rVal = 0.0;` |
|      214 |  421 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),` |
|        - |  422 | `			(void *)&rVal,0);` |
|      214 |  423 | `		if( pOverflow ){` |
|        - |  424 | `			/* php reports no overflow for a float-shaped string however large it` |
|        - |  425 | `			 * is: it was always going to be a double, so no digits were lost. */` |
|      ! 0 |  426 | `			*pOverflow = 0;` |
|      ! 0 |  427 | `		}` |
|      214 |  428 | `		return MemObjRealToIntCap(rVal);` |
|        - |  429 | `	}` |
|        - |  430 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  431 | `	/* A *string* is always read in base 10 by php: "012" is 12, "0x1A" and "0b11"` |
|        - |  432 | `	 * are 0. Only a source *literal* carries a base prefix, and that is decoded by` |
|        - |  433 | `	 * the compiler (PH7_TokenValueToInt64) -- not here. */` |
|  1177380 |  434 | `	SyStrToInt64Ex((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&iVal,0,pOverflow);` |
|  1177380 |  435 | `	return iVal;` |
|   588789 |  436 | `}` |
|        - |  437 | `/*` |
|        - |  438 | ` * Call a magic class method [i.e: __toString(),__toInt(),...]` |
|        - |  439 | ` * Return SXRET_OK if the magic method is available and have been` |
|        - |  440 | ` * successfully called. Any other return value indicates failure.` |
|        - |  441 | ` */` |
|     2679 |  442 | `static sxi32 MemObjCallClassCastMethod(` |
|        - |  443 | `	ph7_vm *pVm,               /* VM that trigger the invocation */` |
|        - |  444 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object] */` |
|        - |  445 | `	const char *zMethod,       /* Magic method name [i.e: __toString] */` |
|        - |  446 | `	sxu32 nLen,                /* Method name length */` |
|        - |  447 | `	ph7_value *pResult         /* OUT: Store the return value of the magic method here */` |
|        - |  448 | `	)` |
|        5 |  449 | `{` |
|        - |  450 | `	ph7_class_method *pMethod;` |
|        - |  451 | `	/* Check if the method is available */` |
|     2684 |  452 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,zMethod,nLen);` |
|     2684 |  453 | `	if( pMethod == 0 ){` |
|        - |  454 | `		/* No such method */` |
|        6 |  455 | `		return SXERR_NOTFOUND;` |
|        - |  456 | `	}` |
|        - |  457 | `	/* Invoke the desired method and hand back ITS status: a magic cast method` |
|        - |  458 | `	 * that threw must not be reported as a successful call, or the caller` |
|        - |  459 | `	 * expands its fallback and the abandoned coercion produces a value (echo` |
|        - |  460 | `	 * printed "Object" after a caught __toString() throw). */` |
|     2680 |  461 | `	return PH7_VmCallClassMethod(&(*pVm),&(*pThis),pMethod,&(*pResult),0,0);` |
|     1343 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * The number an object's own STRING is -- php's cast_object with IS_LONG /` |
|        - |  465 | ` * IS_DOUBLE for the one class that answers those (PH7_CLASS_NUM_AS_STRING).` |
|        - |  466 | ` *` |
|        - |  467 | ` * Both casts read the same text, so this answers the integer and, when the` |
|        - |  468 | `` * caller wants it, writes the float beside it: `(int)$x` on `<c>2.5</c>` is 2`` |
|        - |  469 | `` * and `(float)$x` is 2.5, exactly as the two string conversions would give.`` |
|        - |  470 | ` * Silent: this class HAS an answer, so php raises nothing.` |
|        - |  471 | ` */` |
|        8 |  472 | `static sxi64 MemObjIntFromClassString(ph7_vm *pVm,ph7_class_instance *pThis,ph7_real *pReal)` |
|        1 |  473 | `{` |
|        - |  474 | `	ph7_value sText;` |
|        9 |  475 | `	sxi64 iVal = 0;` |
|        9 |  476 | `	if( pReal ){` |
|        3 |  477 | `		*pReal = 0;` |
|        1 |  478 | `	}` |
|        9 |  479 | `	if( pVm == 0 \|\| pThis == 0 ){` |
|      ! 0 |  480 | `		return 0;` |
|        - |  481 | `	}` |
|        9 |  482 | `	PH7_MemObjInit(pVm,&sText);` |
|        8 |  483 | `	if( MemObjCallClassCastMethod(pVm,pThis,"__toString",sizeof("__toString")-1,&sText)` |
|        9 |  484 | `		== SXRET_OK && (sText.iFlags & MEMOBJ_STRING) ){` |
|        9 |  485 | `		iVal = MemObjStringToInt(&sText,0);` |
|        9 |  486 | `		if( pReal ){` |
|        4 |  487 | `			SyStrToReal((const char *)SyBlobData(&sText.sBlob),` |
|        1 |  488 | `				SyBlobLength(&sText.sBlob),(void *)pReal,0);` |
|        1 |  489 | `		}` |
|        4 |  490 | `	}` |
|        9 |  491 | `	PH7_MemObjRelease(&sText);` |
|        9 |  492 | `	return iVal;` |
|        5 |  493 | `}` |
|        - |  494 | `/*` |
|        - |  495 | ` * Return some kind of integer value which is the best we can` |
|        - |  496 | ` * do at representing the value that pObj describes as an integer.` |
|        - |  497 | ` * If pObj is an integer, then the value is exact. If pObj is` |
|        - |  498 | ` * a floating-point then  the value returned is the integer part.` |
|        - |  499 | ` * If pObj is a string, then we make an attempt to convert it into` |
|        - |  500 | ` * a integer and return that.` |
|        - |  501 | ` * If pObj represents a NULL value, return 0.` |
|        - |  502 | ` */` |
|  1179019 |  503 | `static sxi64 MemObjIntValue(ph7_value *pObj)` |
|        5 |  504 | `{` |
|        - |  505 | `	sxi32 iFlags;` |
|  1179024 |  506 | `	iFlags = pObj->iFlags;` |
|  1179024 |  507 | `	if (iFlags & MEMOBJ_REAL ){` |
|      368 |  508 | `		return MemObjRealToInt(&(*pObj));` |
|  1178660 |  509 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     2863 |  510 | `		return pObj->x.iVal;` |
|  1175802 |  511 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  512 | `		/* php's (int) cast SATURATES an out-of-range numeric string, so the` |
|        - |  513 | `		 * overflow report is deliberately dropped here. Only the string->NUMBER` |
|        - |  514 | `		 * conversion (PH7_MemObjToNumeric) acts on it. */` |
|  1175116 |  515 | `		return MemObjStringToInt(&(*pObj),0);` |
|      691 |  516 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|      589 |  517 | `		return 0;` |
|      105 |  518 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  519 | `		/* php: (int) of an array is 0 when empty, 1 otherwise -- NOT the element` |
|        - |  520 | ``		 * count. PHL returned the count, so `(int)[1,2,3]` was 3. (bool) already`` |
|        - |  521 | `		 * followed php; int/float did not.) */` |
|       36 |  522 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|       36 |  523 | `		sxu32 n = pMap->nEntry;` |
|       36 |  524 | `		PH7_HashmapUnref(pMap);` |
|       36 |  525 | `		return n > 0 ? 1 : 0;` |
|       71 |  526 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  527 | `		/* php has NO __toInt(): casting an object to int warns and yields 1. PH7's` |
|        - |  528 | `		 * __toInt() was an extension that changed the meaning of valid php source` |
|        - |  529 | ``		 * (§10), so `(int)$obj` silently returned user data where php diagnoses.`` |
|        - |  530 | `		 *` |
|        - |  531 | `		 * Two classes are php's own exception -- the curl easy and multi handles,` |
|        - |  532 | `		 * which answer their OBJECT HANDLE and say nothing, because they used to be` |
|        - |  533 | ``		 * resources and `(int)$h` used to be the resource id (PH7_CLASS_HANDLE_ID).`` |
|        - |  534 | `		 * Every door that asks an int of a value comes through here, which is what` |
|        - |  535 | ``		 * makes intval(), settype(), `%d` and array_sum() agree with the cast. */`` |
|       44 |  536 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       44 |  537 | `		if( pInst && pInst->pClass && PH7_ClassCastsToHandleId(pInst->pClass) ){` |
|       21 |  538 | `			sxi64 iId = (sxi64)pInst->nObjId;` |
|       21 |  539 | `			PH7_ClassInstanceUnref(pInst);` |
|       21 |  540 | `			return iId;` |
|        - |  541 | `		}` |
|       24 |  542 | `		if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        - |  543 | `			/* php's SimpleXMLElement casts to the NUMBER ITS TEXT IS, silently. */` |
|        7 |  544 | `			sxi64 iVal = MemObjIntFromClassString(pObj->pVm,pInst,0);` |
|        7 |  545 | `			PH7_ClassInstanceUnref(pInst);` |
|        7 |  546 | `			return iVal;` |
|        - |  547 | `		}` |
|       17 |  548 | `		if( pInst && pInst->pClass ){` |
|       24 |  549 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|       14 |  550 | `				"Object of class %z could not be converted to int",&pInst->pClass->sName);` |
|        7 |  551 | `		}` |
|       17 |  552 | `		PH7_ClassInstanceUnref(pInst);` |
|       17 |  553 | `		return 1;` |
|       30 |  554 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        - |  555 | `		/* php casts a resource to its ID, not to 1: two distinct resources must not` |
|        - |  556 | `		 * compare equal, which they did while every one of them cast to 1. */` |
|       30 |  557 | `		return (sxi64)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  558 | `	}` |
|        - |  559 | `	/* CANT HAPPEN */` |
|      ! 0 |  560 | `	return 0;` |
|   589497 |  561 | `}` |
|        - |  562 | `/*` |
|        - |  563 | ` * Return some kind of real value which is the best we can` |
|        - |  564 | ` * do at representing the value that pObj describes as a real.` |
|        - |  565 | ` * If pObj is a real, then the value is exact.If pObj is an` |
|        - |  566 | ` * integer then the integer  is promoted to real and that value` |
|        - |  567 | ` * is returned.` |
|        - |  568 | ` * If pObj is a string, then we make an attempt to convert it` |
|        - |  569 | ` * into a real and return that.` |
|        - |  570 | ` * If pObj represents a NULL value, return 0.0` |
|        - |  571 | ` */` |
|    20279 |  572 | `static ph7_real MemObjRealValue(ph7_value *pObj)` |
|        5 |  573 | `{` |
|        - |  574 | `	sxi32 iFlags;` |
|    20284 |  575 | `	iFlags = pObj->iFlags;` |
|    20284 |  576 | `	if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  577 | `		return pObj->rVal;` |
|    20284 |  578 | `	}else if (iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     3689 |  579 | `		return (ph7_real)pObj->x.iVal;` |
|    16600 |  580 | `	}else if (iFlags & MEMOBJ_STRING){` |
|        - |  581 | `		SyString sString;` |
|        - |  582 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  583 | `		ph7_real rVal = 0;` |
|        - |  584 | `#else` |
|    16574 |  585 | `		ph7_real rVal = 0.0;` |
|        - |  586 | `#endif` |
|    16574 |  587 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    16574 |  588 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        - |  589 | `			/* Convert as much as we can */` |
|        - |  590 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  591 | `			rVal = MemObjStringToInt(&(*pObj),0);` |
|        - |  592 | `#else` |
|    16570 |  593 | `			SyStrToReal(sString.zString,sString.nByte,(void *)&rVal,0);` |
|        - |  594 | `#endif` |
|     8273 |  595 | `		}` |
|    16574 |  596 | `		return rVal;` |
|       29 |  597 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - |  598 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  599 | `		return 0;` |
|        - |  600 | `#else` |
|       11 |  601 | `		return 0.0;` |
|        - |  602 | `#endif` |
|       19 |  603 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|        - |  604 | `		/* php: (float) of an array is 0.0 when empty, 1.0 otherwise -- see the int` |
|        - |  605 | `		 * branch above. */` |
|      ! 0 |  606 | `		ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      ! 0 |  607 | `		sxu32 n = pMap->nEntry;` |
|      ! 0 |  608 | `		PH7_HashmapUnref(pMap);` |
|      ! 0 |  609 | `		return n > 0 ? (ph7_real)1.0 : (ph7_real)0.0;` |
|       19 |  610 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  611 | `		/* php has NO __toFloat(): casting an object to float warns and yields 1.0. */` |
|       17 |  612 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       17 |  613 | `		if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        3 |  614 | `			ph7_real rV = 0;` |
|        3 |  615 | `			(void)MemObjIntFromClassString(pObj->pVm,pInst,&rV);` |
|        3 |  616 | `			PH7_ClassInstanceUnref(pInst);` |
|        3 |  617 | `			return rV;` |
|        - |  618 | `		}` |
|       14 |  619 | `		if( pInst && pInst->pClass ){` |
|       20 |  620 | `			VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|       12 |  621 | `				"Object of class %z could not be converted to float",&pInst->pClass->sName);` |
|        6 |  622 | `		}` |
|       14 |  623 | `		PH7_ClassInstanceUnref(pInst);` |
|       14 |  624 | `		return (ph7_real)1.0;` |
|        3 |  625 | `	}else if(iFlags & MEMOBJ_RES ){` |
|        3 |  626 | `		return (ph7_real)PH7_VmResourceId(pObj->pVm,pObj->x.pOther);` |
|        - |  627 | `	}` |
|        - |  628 | `	/* NOT REACHED  */` |
|      ! 0 |  629 | `	return 0;` |
|    10135 |  630 | `}` |
|        - |  631 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  632 | `/*` |
|        - |  633 | ` * Post-process a libc-formatted float into php's exact shape (php_gcvt /` |
|        - |  634 | ` * smart_str_append_double semantics): strip the exponent's zero padding` |
|        - |  635 | ` * (libc's 1e+08 becomes php's 1e+8; a zero exponent stays e+0) and, when` |
|        - |  636 | ` * bGeneric is set (%g-style output, including the default float->string` |
|        - |  637 | ` * cast), make an exponent-form mantissa keep a fractional digit` |
|        - |  638 | ` * (1e+20 -> 1.0e+20). zBuf must be NUL-terminated with at least two bytes` |
|        - |  639 | ` * of spare capacity past the NUL. Returns the new length.` |
|        - |  640 | ` * Defined here (not builtin.c) because the float->string cast below needs it` |
|        - |  641 | ` * even when builtin.c's formatting region is compiled out` |
|        - |  642 | ` * (PH7_DISABLE_DISK_IO); the printf family reuses it from PH7_InputFormat.` |
|        - |  643 | ` */` |
|      870 |  644 | `PH7_PRIVATE sxi32 PH7_PhpFloatShape(char *zBuf,sxi32 nLen,int bGeneric)` |
|        4 |  645 | `{` |
|        - |  646 | `	sxi32 iExp,i;` |
|      874 |  647 | `	iExp = nLen - 1;` |
|     6640 |  648 | `	while( iExp > 0 && zBuf[iExp] != 'e' && zBuf[iExp] != 'E' ){` |
|     5770 |  649 | `		iExp--;` |
|        4 |  650 | `	}` |
|      874 |  651 | `	if( iExp <= 0 ){` |
|      752 |  652 | `		return nLen; /* No exponent part (fixed notation) */` |
|        - |  653 | `	}` |
|        - |  654 | `	{` |
|      125 |  655 | `		sxi32 iDig = iExp + 1;` |
|        - |  656 | `		sxi32 iFirst;` |
|      125 |  657 | `		if( zBuf[iDig] == '+' \|\| zBuf[iDig] == '-' ){` |
|      125 |  658 | `			iDig++;` |
|       61 |  659 | `		}` |
|      125 |  660 | `		iFirst = iDig;` |
|      162 |  661 | `		while( zBuf[iFirst] == '0' && iFirst + 1 < nLen` |
|      104 |  662 | `		 && zBuf[iFirst+1] >= '0' && zBuf[iFirst+1] <= '9' ){` |
|       27 |  663 | `			iFirst++;` |
|        1 |  664 | `		}` |
|      125 |  665 | `		if( iFirst > iDig ){` |
|       27 |  666 | `			sxi32 nStrip = iFirst - iDig;` |
|       79 |  667 | `			for( i = iDig ; i + nStrip <= nLen ; i++ ){` |
|       53 |  668 | `				zBuf[i] = zBuf[i+nStrip]; /* moves the NUL too */` |
|       27 |  669 | `			}` |
|       27 |  670 | `			nLen -= nStrip;` |
|       13 |  671 | `		}` |
|        - |  672 | `	}` |
|      125 |  673 | `	if( bGeneric ){` |
|      109 |  674 | `		int bHasDot = 0;` |
|      239 |  675 | `		for( i = 0 ; i < iExp ; i++ ){` |
|      203 |  676 | `			if( zBuf[i] == '.' ){ bHasDot = 1; break; }` |
|       68 |  677 | `		}` |
|      109 |  678 | `		if( !bHasDot ){` |
|      221 |  679 | `			for( i = nLen ; i >= iExp ; i-- ){` |
|      185 |  680 | `				zBuf[i+2] = zBuf[i]; /* moves the NUL too */` |
|       94 |  681 | `			}` |
|       39 |  682 | `			zBuf[iExp] = '.';` |
|       39 |  683 | `			zBuf[iExp+1] = '0';` |
|       39 |  684 | `			nLen += 2;` |
|       18 |  685 | `		}` |
|       53 |  686 | `	}` |
|      125 |  687 | `	return nLen;` |
|      439 |  688 | `}` |
|        - |  689 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        - |  690 | `/*` |
|        - |  691 | ` * Return the string representation of a given ph7_value.` |
|        - |  692 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of a __toString()` |
|        - |  693 | ` * that threw -- the only way this can fail, and the only case in which pOut is` |
|        - |  694 | ` * left without a rendering of pObj.` |
|        - |  695 | ` */` |
|    89292 |  696 | `static sxi32 MemObjStringValue(SyBlob *pOut,ph7_value *pObj,sxu8 bStrictBool)` |
|        5 |  697 | `{` |
|    89297 |  698 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - |  699 | `		/* Handle special floating-point values first */` |
|      580 |  700 | `		if( PH7_IS_NAN(pObj->rVal) ){` |
|       25 |  701 | `			SyBlobAppend(&(*pOut),"NAN",3);` |
|      568 |  702 | `		}else if( PH7_IS_INF(pObj->rVal) ){` |
|       11 |  703 | `			if( pObj->rVal < 0.0 ){` |
|        3 |  704 | `				SyBlobAppend(&(*pOut),"-INF",4);` |
|        2 |  705 | `			}else{` |
|        9 |  706 | `				SyBlobAppend(&(*pOut),"INF",3);` |
|        - |  707 | `			}` |
|        6 |  708 | `		}else{` |
|        - |  709 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  710 | `			/* php's default float->string conversion (echo/concat/cast):` |
|        - |  711 | `			 * zend_gcvt with EG(precision)=14 and an uppercase exponent` |
|        - |  712 | `			 * marker (smart_str_append_double) — 1/3 -> "0.33333333333333",` |
|        - |  713 | `			 * 1e15 -> "1.0E+15", -0.0 -> "-0". libc snprintf supplies` |
|        - |  714 | `			 * correctly-rounded digits; PH7_PhpFloatShape applies php's` |
|        - |  715 | `			 * exponent/fraction quirks. */` |
|        - |  716 | `			char zNum[48]; /* %.14G peaks at ~22 bytes; +2 spare for ".0" */` |
|      546 |  717 | `			sxi32 n = (sxi32)snprintf(zNum,sizeof(zNum),"%.14G",pObj->rVal);` |
|      546 |  718 | `			if( n < 0 \|\| n >= (sxi32)sizeof(zNum) ){` |
|      ! 0 |  719 | `				n = (sxi32)SyStrlen(zNum);` |
|      ! 0 |  720 | `			}` |
|      546 |  721 | `			n = PH7_PhpFloatShape(zNum,n,TRUE);` |
|      546 |  722 | `			SyBlobAppend(&(*pOut),zNum,(sxu32)n);` |
|        - |  723 | `#else` |
|        - |  724 | `			SyBlobFormat(&(*pOut),"%.15g",pObj->rVal);` |
|        - |  725 | `#endif` |
|        4 |  726 | `		}` |
|    89009 |  727 | `	}else if( pObj->iFlags & MEMOBJ_INT ){` |
|    84719 |  728 | `		SyBlobFormat(&(*pOut),"%qd",pObj->x.iVal);` |
|        - |  729 | `		/* %qd (BSD quad) is equivalent to %lld in the libc printf */` |
|    46351 |  730 | `	}else if( pObj->iFlags & MEMOBJ_BOOL ){` |
|      583 |  731 | `		if( bStrictBool ){` |
|        - |  732 | `			/* Actual string cast: true -> "1", false -> "" (like PHP) */` |
|      583 |  733 | `			if( pObj->x.iVal ){` |
|      237 |  734 | `				SyBlobAppend(&(*pOut),"1",sizeof("1")-1);` |
|      117 |  735 | `			}` |
|        - |  736 | `			/* false produces empty string, nothing to append */` |
|      294 |  737 | `		}else{` |
|        - |  738 | `			/* Display path (var_dump, print_r): show TRUE/FALSE */` |
|      ! 0 |  739 | `			if( pObj->x.iVal ){` |
|      ! 0 |  740 | `				SyBlobAppend(&(*pOut),"TRUE",sizeof("TRUE")-1);` |
|      ! 0 |  741 | `			}else{` |
|      ! 0 |  742 | `				SyBlobAppend(&(*pOut),"FALSE",sizeof("FALSE")-1);` |
|        - |  743 | `			}` |
|        5 |  744 | `		}` |
|     3718 |  745 | `	}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      309 |  746 | `		SyBlobAppend(&(*pOut),"Array",sizeof("Array")-1);` |
|      309 |  747 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|     3275 |  748 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        - |  749 | `		ph7_value sResult;` |
|        - |  750 | `		sxi32 rc;` |
|        - |  751 | `		/* Invoke the __toString() method if available */` |
|     2676 |  752 | `		PH7_MemObjInit(pObj->pVm,&sResult);` |
|     2676 |  753 | `		rc = MemObjCallClassCastMethod(pObj->pVm,(ph7_class_instance *)pObj->x.pOther,` |
|        - |  754 | `			"__toString",sizeof("__toString")-1,&sResult);` |
|     2676 |  755 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|        - |  756 | `			/* __toString() threw: php abandons the coercion and propagates. Append` |
|        - |  757 | ``			 * NOTHING -- appending the placeholder here made `echo $o` print`` |
|        - |  758 | `` 			 * "Object" AFTER the catch had already run, and turned the `.=` `` |
|        - |  759 | `			 * lvalue and settype()'s target into that string. Return BEFORE the` |
|        - |  760 | `			 * unref: the caller keeps pObj as it was, so it still owns this` |
|        - |  761 | `			 * instance reference. */` |
|      197 |  762 | `			PH7_MemObjRelease(&sResult);` |
|      197 |  763 | `			return rc;` |
|        - |  764 | `		}` |
|     2484 |  765 | `		if( rc == SXRET_OK && (sResult.iFlags & MEMOBJ_STRING) ){` |
|        - |  766 | ``			/* Expand the method return value, the EMPTY string included: `""` is a`` |
|        - |  767 | `			 * value, and requiring a non-empty one sent` |
|        - |  768 | `` 			 * `__toString(){ return ""; }` down the placeholder path, so `"[$o]"` `` |
|        - |  769 | `			 * read "[Object]" where php reads "[]". php's own guarantee that the` |
|        - |  770 | ``			 * result IS a string is the implicit `string` return type on`` |
|        - |  771 | `			 * __toString (installed at its declaration); the fallback below is now` |
|        - |  772 | `			 * reachable only for a class with no __toString at all -- which only` |
|        - |  773 | `			 * the SILENT coercions get this far with -- or a C-thunk method whose` |
|        - |  774 | `			 * result no return-type check governs. */` |
|     2480 |  775 | `			SyBlobDup(&sResult.sBlob,pOut);` |
|     1241 |  776 | `		}else{` |
|        - |  777 | `			/* Expand "Object": a PHL-internal rendering for the coercions php never` |
|        - |  778 | `			 * performs (array keys, sort comparisons, print_r), never user-visible. */` |
|        6 |  779 | `			SyBlobAppend(&(*pOut),"Object",sizeof("Object")-1);` |
|        - |  780 | `		}` |
|     2484 |  781 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|     2484 |  782 | `		PH7_MemObjRelease(&sResult);` |
|     1691 |  783 | `	}else if(pObj->iFlags & MEMOBJ_RES ){` |
|        - |  784 | `		/* php renders a resource as "Resource id #N" with its sequential id; the` |
|        - |  785 | `		 * old "ResourceID_0x<pointer>" leaked an address and matched nothing. */` |
|       17 |  786 | `		SyBlobFormat(&(*pOut),"Resource id #%u",PH7_VmResourceId(pObj->pVm,pObj->x.pOther));` |
|        8 |  787 | `	}` |
|    89105 |  788 | `	return SXRET_OK;` |
|    44635 |  789 | `}` |
|        - |  790 | `/*` |
|        - |  791 | ` * Return some kind of boolean value which is the best we can do` |
|        - |  792 | ` * at representing the value that pObj describes as a boolean.` |
|        - |  793 | ` * When converting to boolean, the following values are considered FALSE` |
|        - |  794 | ` * (php's exact set):` |
|        - |  795 | ` * NULL` |
|        - |  796 | ` * the boolean FALSE itself.` |
|        - |  797 | ` * the integer 0 (zero).` |
|        - |  798 | ` * the real 0.0 (zero).` |
|        - |  799 | ` * the empty string "" and the string "0" (nothing else: "00", "0.0", " ",` |
|        - |  800 | ` * and "false" are all TRUE in php — the historical PH7 zero-stream and` |
|        - |  801 | ` * "false"/"on"/"yes" special cases changed the meaning of valid PHP source` |
|        - |  802 | ` * and were removed under the §10 PH7-ism policy).` |
|        - |  803 | ` * an array with zero elements.` |
|        - |  804 | ` */` |
|   173065 |  805 | `static sxi32 MemObjIsTruthy(ph7_value *pObj)` |
|        5 |  806 | `{` |
|        - |  807 | `	sxi32 iFlags;` |
|   173070 |  808 | `	iFlags = pObj->iFlags;` |
|   173070 |  809 | `	if (iFlags & MEMOBJ_REAL ){` |
|        - |  810 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|        - |  811 | `		return pObj->rVal ? 1 : 0;` |
|        - |  812 | `#else` |
|        - |  813 | `		/* A NaN is neither zero nor equal to itself, so it is TRUE -- php's` |
|        - |  814 | `		 * answer too, behind the warning PH7_MemObjToBool raises. */` |
|      101 |  815 | `		return pObj->rVal != 0.0 ? 1 : 0;` |
|        - |  816 | `#endif` |
|   172972 |  817 | `	}else if( iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        - |  818 | ``		/* BOOL is here for `empty()`, which asks this of a value of ANY type; the`` |
|        - |  819 | `		 * bool CONVERSION never does (it returns early when the bit is set). */` |
|    36082 |  820 | `		return pObj->x.iVal ? 1 : 0;` |
|   136895 |  821 | `	}else if (iFlags & MEMOBJ_STRING) {` |
|        - |  822 | `		SyString sString;` |
|    37224 |  823 | `		SyStringInitFromBuf(&sString,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|        - |  824 | `		/* php: a string is FALSE iff it is empty or exactly "0" */` |
|    37224 |  825 | `		if( sString.nByte == 0 ){` |
|    28821 |  826 | `			return 0;` |
|        - |  827 | `		}` |
|     8408 |  828 | `		if( sString.nByte == 1 && sString.zString[0] == '0' ){` |
|       26 |  829 | `			return 0;` |
|        - |  830 | `		}` |
|     8384 |  831 | `		return 1;` |
|    99676 |  832 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|    78389 |  833 | `		return 0;` |
|    21292 |  834 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|    19149 |  835 | `		return ((ph7_hashmap *)pObj->x.pOther)->nEntry > 0 ? TRUE : FALSE;` |
|     2148 |  836 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|        - |  837 | `		/* php has NO __toBool(): an object is ALWAYS truthy, with no diagnostic.` |
|        - |  838 | ``		 * PH7's __toBool() could make `if ($obj)` take the other branch, so this`` |
|        - |  839 | `		 * extension changed control flow in valid php source.` |
|        - |  840 | `		 *` |
|        - |  841 | `		 * An INTERNAL class may still install php's cast_object handler for` |
|        - |  842 | `		 * _IS_BOOL, which is a different thing entirely -- it is not reachable` |
|        - |  843 | `		 * from PHP source and php ships exactly one: a zero BcMath\Number is` |
|        - |  844 | ``		 * falsy, so `if ($n)` and `empty($n)` read the VALUE. */`` |
|      318 |  845 | `		int bNative = 1;` |
|      318 |  846 | `		if( PH7_ClassNativeBool((ph7_class_instance *)pObj->x.pOther,&bNative) ){` |
|       22 |  847 | `			return bNative;` |
|        - |  848 | `		}` |
|      298 |  849 | `		return 1;` |
|     1834 |  850 | `	}else if(iFlags & MEMOBJ_RES ){` |
|     1834 |  851 | `		return pObj->x.pOther != 0;` |
|        - |  852 | `	}` |
|        - |  853 | `	/* NOT REACHED */` |
|      ! 0 |  854 | `	return 0;` |
|    86495 |  855 | `}` |
|        - |  856 | `/*` |
|        - |  857 | ` * The same question asked by a CONVERSION, which is about to overwrite the` |
|        - |  858 | ` * payload and so owes it a reference drop. Nothing else about the answer` |
|        - |  859 | `` * differs -- which is the point: `empty()` and `array_filter()`'s default test`` |
|        - |  860 | ` * used to carry a SECOND set of rules (PH7_MemObjIsEmpty), and it disagreed.` |
|        - |  861 | ` */` |
|   117094 |  862 | `static sxi32 MemObjBooleanValue(ph7_value *pObj)` |
|        5 |  863 | `{` |
|   117099 |  864 | `	sxi32 rc = MemObjIsTruthy(&(*pObj));` |
|   117099 |  865 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      172 |  866 | `		PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
|   117015 |  867 | `	}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|      308 |  868 | `		PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|      150 |  869 | `	}` |
|   117099 |  870 | `	return rc;` |
|        5 |  871 | `}` |
|        - |  872 | `/*` |
|        - |  873 | ` * If the ph7_value is of type real,try to make it an integer also.` |
|        - |  874 | ` */` |
|    24172 |  875 | `static sxi32 MemObjTryIntger(ph7_value *pObj)` |
|        5 |  876 | `{` |
|    24177 |  877 | `	pObj->x.iVal = MemObjRealToInt(&(*pObj));` |
|        - |  878 | `  /* Only mark the value as an integer if` |
|        - |  879 | `  **` |
|        - |  880 | `  **    (1) the round-trip conversion real->int->real is a no-op, and` |
|        - |  881 | `  **    (2) The integer is neither the largest nor the smallest` |
|        - |  882 | `  **        possible integer` |
|        - |  883 | `  **` |
|        - |  884 | `  ** The second and third terms in the following conditional enforces` |
|        - |  885 | `  ** the second condition under the assumption that addition overflow causes` |
|        - |  886 | `  ** values to wrap around.  On x86 hardware, the third term is always` |
|        - |  887 | `  ** true and could be omitted.  But we leave it in because other` |
|        - |  888 | `  ** architectures might behave differently.` |
|        - |  889 | `  */` |
|    24172 |  890 | `	if( pObj->rVal ==(ph7_real)pObj->x.iVal && pObj->x.iVal>SMALLEST_INT64` |
|    19005 |  891 | `      && pObj->x.iVal<LARGEST_INT64 ){` |
|    18979 |  892 | `		  pObj->iFlags \|= MEMOBJ_INT;` |
|     9479 |  893 | `	}` |
|    24177 |  894 | `	return SXRET_OK;` |
|        5 |  895 | `}` |
|        - |  896 | `/*` |
|        - |  897 | ` * Convert a ph7_value to type integer.Invalidate any prior representations.` |
|        - |  898 | ` */` |
|  4418492 |  899 | `PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj)` |
|        5 |  900 | `{` |
|  4418497 |  901 | `	if( (pObj->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  902 | `		/* Preform the conversion */` |
|  1179024 |  903 | `		pObj->x.iVal = MemObjIntValue(&(*pObj));` |
|        - |  904 | `		/* Invalidate any prior representations */` |
|  1179024 |  905 | `		SyBlobRelease(&pObj->sBlob);` |
|  1179024 |  906 | `		MemObjSetType(pObj,MEMOBJ_INT);` |
|   589492 |  907 | `	}` |
|  4418497 |  908 | `	return SXRET_OK;` |
|        5 |  909 | `}` |
|        - |  910 | `/*` |
|        - |  911 | ` * Convert a ph7_value to type real (Try to get an integer representation also).` |
|        - |  912 | ` * Invalidate any prior representations` |
|        - |  913 | ` */` |
|    23891 |  914 | `PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj)` |
|        5 |  915 | `{` |
|    23896 |  916 | `	if((pObj->iFlags & MEMOBJ_REAL) == 0 ){` |
|        - |  917 | `		/* Preform the conversion */` |
|    20284 |  918 | `		pObj->rVal = MemObjRealValue(&(*pObj));` |
|        - |  919 | `		/* Invalidate any prior representations */` |
|    20284 |  920 | `		SyBlobRelease(&pObj->sBlob);` |
|    20284 |  921 | `		MemObjSetType(pObj,MEMOBJ_REAL);` |
|        - |  922 | `		/* Try to get an integer representation */` |
|    20284 |  923 | `		MemObjTryIntger(&(*pObj));` |
|    10130 |  924 | `	}` |
|    23896 |  925 | `	return SXRET_OK;` |
|        5 |  926 | `}` |
|        - |  927 | `/*` |
|        - |  928 | ` * Convert a ph7_value to type boolean.Invalidate any prior representations.` |
|        - |  929 | ` */` |
|   171563 |  930 | `static sxi32 MemObjToBoolQuiet(ph7_value *pObj)` |
|        5 |  931 | `{` |
|   171568 |  932 | `	if( (pObj->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        - |  933 | `		/* Preform the conversion */` |
|   117099 |  934 | `		pObj->x.iVal = MemObjBooleanValue(&(*pObj));` |
|        - |  935 | `		/* Invalidate any prior representations */` |
|   117099 |  936 | `		SyBlobRelease(&pObj->sBlob);` |
|   117099 |  937 | `		MemObjSetType(pObj,MEMOBJ_BOOL);` |
|    58512 |  938 | `	}` |
|   171568 |  939 | `	return SXRET_OK;` |
|        5 |  940 | `}` |
|        - |  941 | `/*` |
|        - |  942 | ` * The same conversion where a php PROGRAM asked for it, which is every` |
|        - |  943 | `` * truthiness site there is: `(bool)`, `if`, `!`, `&&`, the ternary, `empty()`,`` |
|        - |  944 | `` * `boolval()`, `settype()`, a `bool` parameter internal or userland,`` |
|        - |  945 | `` * `array_filter`'s default test. php 8.5 warns from all of them when the value`` |
|        - |  946 | `` * is a NaN -- `unexpected NAN value was coerced to bool` -- and answers TRUE.`` |
|        - |  947 | ` *` |
|        - |  948 | `` * A COMPARISON is not one of them: `NAN == true` is silent in php, and it`` |
|        - |  949 | ` * reaches the same conversion, which is why the quiet form above exists.` |
|        - |  950 | ` */` |
|    91630 |  951 | `PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj)` |
|        5 |  952 | `{` |
|    91630 |  953 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|    45588 |  954 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|       21 |  955 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        - |  956 | `			"unexpected NAN value was coerced to bool");` |
|       10 |  957 | `	}` |
|    91635 |  958 | `	return MemObjToBoolQuiet(&(*pObj));` |
|        5 |  959 | `}` |
|        - |  960 | `/*` |
|        - |  961 | ` * Convert a ph7_value to type string.Prior representations are NOT invalidated.` |
|        - |  962 | ` */` |
|  5329545 |  963 | `PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj)` |
|        5 |  964 | `{` |
|  5329550 |  965 | `	sxi32 rc = SXRET_OK;` |
|  5329550 |  966 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  967 | `		/* Perform the conversion */` |
|    88899 |  968 | `		SyBlobReset(&pObj->sBlob); /* Reset the internal buffer */` |
|    88899 |  969 | `		rc = MemObjStringValue(&pObj->sBlob,&(*pObj),TRUE);` |
|    88899 |  970 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|        - |  971 | `			/* A __toString() that threw: the coercion is abandoned, so the value` |
|        - |  972 | `			 * keeps its own type (and its instance reference — MemObjStringValue` |
|        - |  973 | ``			 * skipped the unref for exactly this). php's `$o .= "x"` likewise`` |
|        - |  974 | `			 * leaves $o holding the object after the throw is caught. */` |
|      197 |  975 | `			return rc;` |
|        - |  976 | `		}` |
|    88707 |  977 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    44335 |  978 | `	}` |
|  5329358 |  979 | `	return rc;` |
|  2664071 |  980 | `}` |
|        - |  981 | `/*` |
|        - |  982 | ` * php's cast_object handler with IS_STRING: an object whose class declares no` |
|        - |  983 | ` * __toString() cannot be coerced, and php answers the CATCHABLE` |
|        - |  984 | ` *   Error: Object of class X could not be converted to string` |
|        - |  985 | ` * PH7 instead expanded the literal placeholder "Object" (a PH7-ism the old` |
|        - |  986 | `` * comment attributed to the language manual), so `echo $o`, `"$o"`,`` |
|        - |  987 | `` * `(string)$o` and `"x".$o` all produced a six-byte string where php throws —`` |
|        - |  988 | ` * a silent wrong answer that survived every arity and type check. The int and` |
|        - |  989 | ` * float casts have diagnosed php's way for a while (MemObjIntValue /` |
|        - |  990 | ` * MemObjRealValue warn "could not be converted to int/float"); only the string` |
|        - |  991 | ` * cast still carried the placeholder.` |
|        - |  992 | ` *` |
|        - |  993 | ` * The object is left UNTOUCHED: php's throw abandons the coercion, so the` |
|        - |  994 | `` * lvalue that reached a `$o .= "x"` or a settype($o,'string') still holds its`` |
|        - |  995 | ` * object afterwards. Every caller either routes the status (the opcode sites,` |
|        - |  996 | ` * via PH7_DISPATCH_TOSTRING_RC) or records it on its call context (the builtin` |
|        - |  997 | ` * sites: echo/print/settype), and none of them reads the value back. The` |
|        - |  998 | ` * settype() site then blanks its target itself, because php's` |
|        - |  999 | ` * convert_to_string() has already done so by the time the Error escapes.` |
|        - | 1000 | ` */` |
|      610 | 1001 | `static sxi32 MemObjThrowNotStringable(ph7_value *pObj)` |
|        5 | 1002 | `{` |
|      615 | 1003 | `	ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        - | 1004 | `	SyBlob sMsg;` |
|      615 | 1005 | `	SyBlobInit(&sMsg,&pObj->pVm->sAllocator);` |
|      615 | 1006 | `	SyBlobFormat(&sMsg,"Object of class %z could not be converted to string",` |
|      610 | 1007 | `		&pInst->pClass->sName);` |
|        - | 1008 | `	/* VmThrowBuiltinError consumes (releases) sMsg */` |
|      615 | 1009 | `	return VmThrowBuiltinError(pObj->pVm,"Error",sizeof("Error")-1,&sMsg);` |
|        5 | 1010 | `}` |
|        - | 1011 | `/*` |
|        - | 1012 | ` * TRUE when a user-visible string coercion of pObj must throw instead: pObj is` |
|        - | 1013 | ` * an object and its class has no __toString(). Inherited and trait methods` |
|        - | 1014 | ` * count -- PH7_ClassExtractMethod walks the same chain the call would.` |
|        - | 1015 | ` */` |
|   139616 | 1016 | `PH7_PRIVATE int PH7_MemObjIsNotStringable(ph7_value *pObj)` |
|        5 | 1017 | `{` |
|        - | 1018 | `	ph7_class_instance *pInst;` |
|   139621 | 1019 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 \|\| pObj->pVm == 0 ){` |
|   136190 | 1020 | `		return FALSE;` |
|        - | 1021 | `	}` |
|     3436 | 1022 | `	pInst = (ph7_class_instance *)pObj->x.pOther;` |
|     3436 | 1023 | `	if( pInst == 0 \|\| pInst->pClass == 0 ){` |
|      ! 0 | 1024 | `		return FALSE;` |
|        - | 1025 | `	}` |
|     3436 | 1026 | `	return PH7_ClassExtractMethod(pInst->pClass,"__toString",sizeof("__toString")-1) == 0;` |
|    69477 | 1027 | `}` |
|        - | 1028 | `/*` |
|        - | 1029 | ` * User-visible array->string coercion. php emits an E_WARNING` |
|        - | 1030 | ` * "Array to string conversion" wherever an ARRAY is coerced to a string FOR` |
|        - | 1031 | `` * THE USER -- echo/print, concatenation and `.=`, the (string) cast, string`` |
|        - | 1032 | `` * interpolation "$arr", a variable-variable NAME `$$arr`, printf/sprintf %s,`` |
|        - | 1033 | ` * implode(), and settype($x,'string') -- but it stays SILENT for the internal` |
|        - | 1034 | ` * coercions that merely format a value for inspection or use it as a lookup` |
|        - | 1035 | ` * key (print_r/var_export/serialize, array-key canonicalisation, sort` |
|        - | 1036 | `` * comparisons, and the `ph7_value_to_string` embedder API). Those sites keep`` |
|        - | 1037 | ` * the bare PH7_MemObjToString; the user-visible ones call this instead.` |
|        - | 1038 | ` *` |
|        - | 1039 | ` * Behaviour is otherwise identical to PH7_MemObjToString: a no-op when pObj is` |
|        - | 1040 | ` * already a string. The warning routes through pObj->pVm, which every VM-owned` |
|        - | 1041 | ` * ph7_value carries.` |
|        - | 1042 | ` *` |
|        - | 1043 | ` * The OBJECT side is the other half of "user-visible": a class with no` |
|        - | 1044 | ` * __toString() throws php's catchable Error here (MemObjThrowNotStringable)` |
|        - | 1045 | ` * and the value is left alone, while the SILENT internal coercions keep` |
|        - | 1046 | ` * rendering it -- so an array key, a sort comparison or print_r never throws,` |
|        - | 1047 | ` * exactly as php never throws for them.` |
|        - | 1048 | ` *` |
|        - | 1049 | ` * Returns SXRET_OK, or the PH7_EXCEPTION/PH7_ABORT status of the throw.` |
|        - | 1050 | ` */` |
|  1916303 | 1051 | `PH7_PRIVATE sxi32 PH7_MemObjToStringUV(ph7_value *pObj)` |
|        5 | 1052 | `{` |
|  1916308 | 1053 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|  1835248 | 1054 | `		return SXRET_OK;` |
|        - | 1055 | `	}` |
|    81065 | 1056 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) && pObj->pVm ){` |
|      279 | 1057 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|      136 | 1058 | `	}` |
|        - | 1059 | `	/* php 8.5's other coercion warning, and it rides HERE for the same reason` |
|        - | 1060 | `	 * that one does: this is the conversion a program asked for -- a cast, echo,` |
|        - | 1061 | ``	 * concatenation, interpolation, a `string` parameter -- and not the internal`` |
|        - | 1062 | `	 * one a comparison or a debug renderer makes. A NaN is the only float that` |
|        - | 1063 | `	 * warns; INF and -INF spell themselves out in silence. */` |
|    81065 | 1064 | `	if( (pObj->iFlags & MEMOBJ_REAL) && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|       17 | 1065 | `		PH7_VmThrowError(pObj->pVm,0,PH7_CTX_WARNING,` |
|        - | 1066 | `			"unexpected NAN value was coerced to string");` |
|        8 | 1067 | `	}` |
|    81065 | 1068 | `	if( PH7_MemObjIsNotStringable(pObj) ){` |
|      615 | 1069 | `		return MemObjThrowNotStringable(pObj);` |
|        - | 1070 | `	}` |
|    80455 | 1071 | `	return PH7_MemObjToString(pObj);` |
|   956234 | 1072 | `}` |
|        - | 1073 | `/*` |
|        - | 1074 | ` * Nullify a ph7_value.In other words invalidate any prior` |
|        - | 1075 | ` * representation.` |
|        - | 1076 | ` */` |
|        2 | 1077 | `PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj)` |
|        1 | 1078 | `{` |
|        3 | 1079 | `	return PH7_MemObjRelease(pObj);` |
|        1 | 1080 | `}` |
|        - | 1081 | `/*` |
|        - | 1082 | ` * Convert a ph7_value to type array.Invalidate any prior representations.` |
|        - | 1083 | `  * According to the PHP language reference manual.` |
|        - | 1084 | `  *   For any of the types: integer, float, string, boolean converting a value` |
|        - | 1085 | `  *   to an array results in an array with a single element with index zero` |
|        - | 1086 | `  *   and the value of the scalar which was converted.` |
|        - | 1087 | `  */` |
|     7858 | 1088 | `PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj)` |
|        5 | 1089 | `{` |
|     7863 | 1090 | `	if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 1091 | `		ph7_hashmap *pMap;` |
|        - | 1092 | `		/* Allocate a new hashmap instance */` |
|     7142 | 1093 | `		pMap = PH7_NewHashmap(pObj->pVm,0,0);` |
|     7142 | 1094 | `		if( pMap == 0 ){` |
|      ! 0 | 1095 | `			return SXERR_MEM;` |
|        - | 1096 | `		}` |
|     7142 | 1097 | `		if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|        - | 1098 | `			/*` |
|        - | 1099 | `			 * According to the PHP language reference manual.` |
|        - | 1100 | `			 *   For any of the types: integer, float, string, boolean converting a value` |
|        - | 1101 | `			 *   to an array results in an array with a single element with index zero` |
|        - | 1102 | `			 *   and the value of the scalar which was converted.` |
|        - | 1103 | `			 */` |
|      756 | 1104 | `			if( pObj->iFlags & MEMOBJ_OBJ ){` |
|      728 | 1105 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      724 | 1106 | `				if( pInst && pObj->pVm->pClosureClass` |
|      728 | 1107 | `				 && pInst->pClass == pObj->pVm->pClosureClass ){` |
|        - | 1108 | `					/* php's convert_to_array tests for a Closure FIRST, ahead of the` |
|        - | 1109 | `					 * property handler, and wraps it the way it wraps a scalar:` |
|        - | 1110 | ``					 * `(array)$closure` is `[0 => $closure]`, not the shape`` |
|        - | 1111 | `					 * var_dump shows. Closure is final, so the exact-class test is` |
|        - | 1112 | `					 * php's (Z_OBJCE_P(op) == zend_ce_closure). */` |
|        3 | 1113 | `					PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        2 | 1114 | `				}else{` |
|        - | 1115 | `					/* Object cast */` |
|      726 | 1116 | `					PH7_ClassInstanceToHashmap(pInst,pMap);` |
|        - | 1117 | `				}` |
|      366 | 1118 | `			}else{` |
|        - | 1119 | `				/* Insert a single element */` |
|       30 | 1120 | `				PH7_HashmapInsert(pMap,0/* Automatic index assign */,&(*pObj));` |
|        - | 1121 | `			}` |
|      756 | 1122 | `			SyBlobRelease(&pObj->sBlob);` |
|      376 | 1123 | `		}` |
|        - | 1124 | `		/* Invalidate any prior representation */` |
|     7142 | 1125 | `		PH7_MemObjRelease(pObj);` |
|     7142 | 1126 | `		MemObjSetType(pObj,MEMOBJ_HASHMAP);` |
|     7142 | 1127 | `		pObj->x.pOther = pMap;` |
|     3521 | 1128 | `	}` |
|     7863 | 1129 | `	return SXRET_OK;` |
|     3839 | 1130 | `}` |
|        - | 1131 | `/* Per-entry callback for the array branch of the (object) cast: add one dynamic` |
|        - | 1132 | ` * property to the target stdClass, named by the array key (rendered as a string,` |
|        - | 1133 | ` * matching PHP) and holding a copy of the value. */` |
|        - | 1134 | `struct VmObjCastData { ph7_vm *pVm; ph7_class_instance *pStd; };` |
|      184 | 1135 | `static int VmArrayToObjectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        4 | 1136 | `{` |
|      188 | 1137 | `	struct VmObjCastData *pData = (struct VmObjCastData *)pUserData;` |
|        - | 1138 | `	ph7_value *pSlot;` |
|        - | 1139 | `	/* pKey and pValue are walk-owned temporaries (PH7_HashmapWalk passes pointers to` |
|        - | 1140 | `	 * its own stack-local sKey/sValue, not slots inside pVm->aMemObj), so they survive` |
|        - | 1141 | `	 * the slot reservation inside PH7_VmCreateDynamicAttr — no snapshot needed. pKey is` |
|        - | 1142 | `	 * safe to coerce in place. */` |
|      188 | 1143 | `	PH7_MemObjToString(pKey);` |
|      280 | 1144 | `	pSlot = PH7_VmCreateDynamicAttr(pData->pVm,pData->pStd,` |
|      184 | 1145 | `		(const char *)SyBlobData(&pKey->sBlob),(sxu32)SyBlobLength(&pKey->sBlob),0);` |
|      188 | 1146 | `	if( pSlot ){` |
|      188 | 1147 | `		PH7_MemObjStore(pValue,pSlot);` |
|       92 | 1148 | `	}` |
|      188 | 1149 | `	return SXRET_OK;` |
|        4 | 1150 | `}` |
|        - | 1151 | `/*` |
|        - | 1152 | ` * Convert a ph7_value to type object, invalidating any prior representation.` |
|        - | 1153 | ` * The new object is a (PHP-empty) stdClass populated with dynamic properties,` |
|        - | 1154 | ` * matching PHP's (object) cast:` |
|        - | 1155 | ` *   - array  -> one property per entry (key rendered as a string -> name).` |
|        - | 1156 | ` *   - scalar -> a single property named "scalar".` |
|        - | 1157 | ` *   - null   -> an empty stdClass (no properties).` |
|        - | 1158 | ` *   - object -> returned unchanged (the MEMOBJ_OBJ guard below).` |
|        - | 1159 | ` */` |
|      118 | 1160 | `PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj)` |
|        5 | 1161 | `{` |
|      123 | 1162 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 1163 | `		ph7_class_instance *pStd;` |
|        - | 1164 | `		ph7_class *pClass;` |
|        - | 1165 | `		ph7_vm *pVm;` |
|        - | 1166 | `		/* Point to the underlying VM + the stdClass */` |
|      123 | 1167 | `		pVm = pObj->pVm;` |
|      182 | 1168 | `		pClass = pVm->pStdClass ? pVm->pStdClass` |
|       59 | 1169 | `			: PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|      123 | 1170 | `		if( pClass == 0 ){` |
|        - | 1171 | `			/* Can't happen,load null instead */` |
|      ! 0 | 1172 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 | 1173 | `			return SXRET_OK;` |
|        - | 1174 | `		}` |
|        - | 1175 | `		/* Instanciate a new (empty) stdClass object */` |
|      123 | 1176 | `		pStd = PH7_NewClassInstance(pVm,pClass);` |
|      123 | 1177 | `		if( pStd == 0 ){` |
|        - | 1178 | `			/* Out of memory */` |
|      ! 0 | 1179 | `			PH7_MemObjRelease(pObj);` |
|      ! 0 | 1180 | `			return SXRET_OK;` |
|        - | 1181 | `		}` |
|      123 | 1182 | `		pStd->iRef = 1;` |
|      123 | 1183 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1184 | `			/* Array: one dynamic property per entry. */` |
|        - | 1185 | `			struct VmObjCastData sData;` |
|      108 | 1186 | `			sData.pVm = pVm;` |
|      108 | 1187 | `			sData.pStd = pStd;` |
|      108 | 1188 | `			ph7_array_walk(pObj,VmArrayToObjectWalk,&sData);` |
|       68 | 1189 | `		}else if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 1190 | `			/* Scalar (int/float/bool/string): a single "scalar" property. */` |
|       14 | 1191 | `			ph7_value *pSlot = PH7_VmCreateDynamicAttr(pVm,pStd,"scalar",sizeof("scalar")-1,0);` |
|       14 | 1192 | `			if( pSlot ){` |
|       14 | 1193 | `				PH7_MemObjStore(pObj,pSlot);` |
|        6 | 1194 | `			}` |
|        6 | 1195 | `		}` |
|        - | 1196 | `		/* (A NULL source yields an empty stdClass — nothing to populate.) */` |
|        - | 1197 | `		/* Invalidate any prior representation */` |
|      123 | 1198 | `		PH7_MemObjRelease(pObj);` |
|        - | 1199 | `		/* Save the new instance */` |
|      123 | 1200 | `		pObj->x.pOther = pStd;` |
|      123 | 1201 | `		MemObjSetType(pObj,MEMOBJ_OBJ);` |
|       59 | 1202 | `	}` |
|      123 | 1203 | `	return SXRET_OK;` |
|       64 | 1204 | `}` |
|        - | 1205 | `/*` |
|        - | 1206 | ` * Return a pointer to the appropriate convertion method associated` |
|        - | 1207 | ` * with the given type.` |
|        - | 1208 | ` * Note on type juggling.` |
|        - | 1209 | ` * Accoding to the PHP language reference manual` |
|        - | 1210 | ` *  PHP does not require (or support) explicit type definition in variable` |
|        - | 1211 | ` *  declaration; a variable's type is determined by the context in which` |
|        - | 1212 | ` *  the variable is used. That is to say, if a string value is assigned` |
|        - | 1213 | ` *  to variable $var, $var becomes a string. If an integer value is then` |
|        - | 1214 | ` *  assigned to $var, it becomes an integer.` |
|        - | 1215 | ` */` |
|   100360 | 1216 | `PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags)` |
|        5 | 1217 | `{` |
|   100365 | 1218 | `	if( iFlags & MEMOBJ_STRING ){` |
|      108 | 1219 | `		return PH7_MemObjToString;` |
|   100261 | 1220 | `	}else if( iFlags & MEMOBJ_INT ){` |
|   100137 | 1221 | `		return PH7_MemObjToInteger;` |
|      128 | 1222 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|       59 | 1223 | `		return PH7_MemObjToReal;` |
|       72 | 1224 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|       62 | 1225 | `		return PH7_MemObjToBool;` |
|       11 | 1226 | `	}else if( iFlags & MEMOBJ_HASHMAP ){` |
|       11 | 1227 | `		return PH7_MemObjToHashmap;` |
|      ! 0 | 1228 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 1229 | `		return PH7_MemObjToObject;` |
|      ! 0 | 1230 | `	}else if( iFlags & MEMOBJ_NULL ){` |
|        - | 1231 | ``		/* `null` is a type, not a weak-coercion target: never silently cast a`` |
|        - | 1232 | ``		 * value to null for a standalone `null` type hint. Return/property`` |
|        - | 1233 | `		 * enforcement reject a non-null value before reaching here; this guards` |
|        - | 1234 | `		 * the parameter default-value path from quietly nulling a non-null` |
|        - | 1235 | `		 * default. */` |
|      ! 0 | 1236 | `		return 0;` |
|        - | 1237 | `	}` |
|        - | 1238 | `	/* NULL cast */` |
|      ! 0 | 1239 | `	return PH7_MemObjToNull;` |
|    50185 | 1240 | `}` |
|        - | 1241 | `/*` |
|        - | 1242 | ` * Return TRUE only if the entire string held by pValue (optionally surrounded` |
|        - | 1243 | ` * by whitespace, with an optional sign) is a well-formed PHP numeric string.` |
|        - | 1244 | ` * This mirrors PHP's is_numeric_string grammar used for is_numeric() and the` |
|        - | 1245 | ` * loose-comparison numeric gate:` |
|        - | 1246 | ` *` |
|        - | 1247 | ` *   [ws] [sign] ( D+ [.D*] \| .D+ ) [ (e\|E) [sign] D+ ] [ws]   (whole string)` |
|        - | 1248 | ` *` |
|        - | 1249 | ` * Implemented directly rather than via SyStrIsNumeric — which returns OK on any` |
|        - | 1250 | ` * numeric PREFIX (so it wrongly accepts "10abc"/"0x1A"/"0b101") and requires a` |
|        - | 1251 | ` * leading digit (so it wrongly rejects ".5"/"-.5", valid in PHP). Unlike a` |
|        - | 1252 | ` * strtod-based classifier this needs no NUL-terminated buffer. Returns FALSE for` |
|        - | 1253 | ` * a non-string value.` |
|        - | 1254 | ` */` |
|        - | 1255 | `/*` |
|        - | 1256 | ` * Scan php's numeric-string grammar and report the longest numeric PREFIX.` |
|        - | 1257 | ` * Returns 1 when the string starts with a number, 0 when nothing numeric is` |
|        - | 1258 | ` * there at all ("abc", "", ".", "e5"). On success *pzTail points just past the` |
|        - | 1259 | ` * prefix, so the caller can tell a fully numeric string ("1e3", " 5 ") from a` |
|        - | 1260 | ` * merely leading-numeric one ("5abc", "1e", "0x1A") -- php warns on the latter` |
|        - | 1261 | ` * and rejects a string with no prefix outright.` |
|        - | 1262 | ` */` |
|  1603156 | 1263 | `PH7_PRIVATE int PH7_MemObjStringNumericPrefix(ph7_value *pValue,const char **pzTail)` |
|        5 | 1264 | `{` |
|        - | 1265 | `	const char *z, *zEnd;` |
|        - | 1266 | `	sxu32 n;` |
|  1603161 | 1267 | `	int bDigit = 0;` |
|  1603161 | 1268 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 1269 | `		return 0;` |
|        - | 1270 | `	}` |
|  1603161 | 1271 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|  1603161 | 1272 | `	n = SyBlobLength(&pValue->sBlob);` |
|  1603161 | 1273 | `	if( n == 0 ){` |
|     2043 | 1274 | `		return 0;` |
|        - | 1275 | `	}` |
|  1601123 | 1276 | `	zEnd = z + n;` |
|  1606003 | 1277 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){` |
|     4884 | 1278 | `		z++;` |
|        4 | 1279 | `	}` |
|  1601123 | 1280 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|      893 | 1281 | `		z++;` |
|      444 | 1282 | `	}` |
|  4412533 | 1283 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|  2811415 | 1284 | `		z++; bDigit = 1;` |
|        5 | 1285 | `	}` |
|  1601123 | 1286 | `	if( z < zEnd && z[0] == '.' ){` |
|      943 | 1287 | `		z++;` |
|     2477 | 1288 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|     1538 | 1289 | `			z++; bDigit = 1;` |
|        4 | 1290 | `		}` |
|      470 | 1291 | `	}` |
|        - | 1292 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|  1601123 | 1293 | `	if( !bDigit ){` |
|   412499 | 1294 | `		return 0;` |
|        - | 1295 | `	}` |
|        - | 1296 | `	/* Optional exponent — only joins the prefix if it carries a digit. "1e" has` |
|        - | 1297 | `	 * the numeric prefix "1" with the 'e' left in the tail, exactly as php reads it. */` |
|  1188629 | 1298 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|      344 | 1299 | `		const char *zExp = z;` |
|      344 | 1300 | `		z++;` |
|      344 | 1301 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       23 | 1302 | `			z++;` |
|       11 | 1303 | `		}` |
|      344 | 1304 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       20 | 1305 | `			z = zExp;` |
|       11 | 1306 | `		}else{` |
|      766 | 1307 | `			while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      444 | 1308 | `				z++;` |
|        4 | 1309 | `			}` |
|        - | 1310 | `		}` |
|      170 | 1311 | `	}` |
|  1188629 | 1312 | `	if( pzTail ){` |
|  1188589 | 1313 | `		*pzTail = z;` |
|   594268 | 1314 | `	}` |
|  1188629 | 1315 | `	return 1;` |
|   801550 | 1316 | `}` |
|        - | 1317 | `/*` |
|        - | 1318 | ` * TRUE only if the WHOLE string is a well-formed php numeric string` |
|        - | 1319 | ` * (trailing whitespace allowed, nothing else).` |
|        - | 1320 | ` */` |
|   418013 | 1321 | `PH7_PRIVATE int PH7_MemObjStringIsNumeric(ph7_value *pValue)` |
|        5 | 1322 | `{` |
|   418018 | 1323 | `	const char *zTail = 0, *zEnd;` |
|   418018 | 1324 | `	if( !PH7_MemObjStringNumericPrefix(pValue,&zTail) ){` |
|   412263 | 1325 | `		return 0;` |
|        - | 1326 | `	}` |
|     5760 | 1327 | `	zEnd = (const char *)SyBlobData(&pValue->sBlob) + SyBlobLength(&pValue->sBlob);` |
|     5834 | 1328 | `	while( zTail < zEnd && (unsigned char)zTail[0] < 0xc0 && SyisSpace(zTail[0]) ){` |
|       77 | 1329 | `		zTail++;` |
|        3 | 1330 | `	}` |
|     5760 | 1331 | `	return zTail == zEnd ? 1 : 0;` |
|   208995 | 1332 | `}` |
|        - | 1333 | `/*` |
|        - | 1334 | ` * php's three-way is_numeric_string classification, which only the loose` |
|        - | 1335 | ` * string/string comparison needs to tell apart. Returns TRUE when pObj is a` |
|        - | 1336 | ` * wholly-numeric INTEGER-shaped string -- the shape php reads as a long -- and` |
|        - | 1337 | ` * then reports through *piOverflow whether its digit run ran PAST the int64` |
|        - | 1338 | ` * range (1 positive side, -1 negative, 0 fits) and through *prVal the double` |
|        - | 1339 | ` * those bytes convert to when it did.` |
|        - | 1340 | ` *` |
|        - | 1341 | ` * FALSE covers a value that is not a string, a string that is not wholly` |
|        - | 1342 | ` * numeric, and a FLOAT-shaped one -- php reports no overflow for that last case` |
|        - | 1343 | ` * however large it is, because it was always going to be a double, so making it` |
|        - | 1344 | ` * one lost no digits.` |
|        - | 1345 | ` *` |
|        - | 1346 | ` * Reads pObj without converting it: the comparison still needs the operand` |
|        - | 1347 | ` * intact when this says no.` |
|        - | 1348 | ` */` |
|      856 | 1349 | `static int MemObjStringIntShape(ph7_value *pObj,int *piOverflow,ph7_real *prVal)` |
|        3 | 1350 | `{` |
|      859 | 1351 | `	const char *zTail = 0;` |
|      859 | 1352 | `	int iOverflow = 0;` |
|      859 | 1353 | `	*piOverflow = 0;` |
|      859 | 1354 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 \|\| !PH7_MemObjStringIsNumeric(pObj) ){` |
|      104 | 1355 | `		return FALSE;` |
|        - | 1356 | `	}` |
|      757 | 1357 | `	if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|      ! 0 | 1358 | `		return FALSE;` |
|        - | 1359 | `	}` |
|        - | 1360 | `	/* Integer-shaped only: a '.' or a complete exponent inside the prefix makes` |
|        - | 1361 | `	 * it a float, exactly as PH7_MemObjToNumeric decides the type. */` |
|      757 | 1362 | `	if( MemObjNumericPrefixIsFloat(pObj,zTail) ){` |
|       82 | 1363 | `		return FALSE;` |
|        - | 1364 | `	}` |
|      677 | 1365 | `	MemObjStringToInt(pObj,&iOverflow);` |
|      677 | 1366 | `	*piOverflow = iOverflow;` |
|      677 | 1367 | `	if( iOverflow != 0 && prVal ){` |
|      335 | 1368 | `		SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)prVal,0);` |
|      167 | 1369 | `	}` |
|      677 | 1370 | `	return TRUE;` |
|      427 | 1371 | `}` |
|        - | 1372 | `/*` |
|        - | 1373 | ` * Check whether the ph7_value is numeric [i.e: int/float/bool] or looks` |
|        - | 1374 | ` * like a numeric number [i.e: if the ph7_value is of type string.].` |
|        - | 1375 | ` * Return TRUE if numeric.FALSE otherwise.` |
|        - | 1376 | ` */` |
|   330711 | 1377 | `PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj)` |
|        5 | 1378 | `{` |
|   330716 | 1379 | `	if( pObj->iFlags & ( MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|     5289 | 1380 | `		return TRUE;` |
|   325432 | 1381 | `	}else if( pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     8829 | 1382 | `		return FALSE;` |
|   316608 | 1383 | `	}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 1384 | `		/* TRUE only if the whole string is a well-formed PHP numeric string. */` |
|   316608 | 1385 | `		return PH7_MemObjStringIsNumeric(pObj) ? TRUE : FALSE;` |
|        - | 1386 | `	}` |
|        - | 1387 | `	/* NOT REACHED */` |
|      ! 0 | 1388 | `	return FALSE;` |
|   165349 | 1389 | `}` |
|        - | 1390 | `/*` |
|        - | 1391 | ` * Check whether the ph7_value is empty.Return TRUE if empty.` |
|        - | 1392 | ` * FALSE otherwise.` |
|        - | 1393 | ` * An ph7_value is considered empty if the following are true:` |
|        - | 1394 | ` * NULL value.` |
|        - | 1395 | ` * Boolean FALSE.` |
|        - | 1396 | ` * Integer/Float with a 0 (zero) value.` |
|        - | 1397 | ` * An empty string or a stream of 0 (zero) [i.e: "0","00","000",...].` |
|        - | 1398 | ` * An empty array.` |
|        - | 1399 | ` * NOTE` |
|        - | 1400 | ` *  OBJECT VALUE MUST NOT BE MODIFIED.` |
|        - | 1401 | ` */` |
|    55971 | 1402 | `PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj)` |
|        5 | 1403 | `{` |
|        - | 1404 | ``	/* php's `empty($x)` is `!zend_is_true($x)` -- the same question the bool cast`` |
|        - | 1405 | `	 * asks, and this used to answer it with rules of its own. They disagreed on a` |
|        - | 1406 | ``	 * string of MORE THAN ONE zero: the old walk called every `"0"` run empty, so`` |
|        - | 1407 | ``	 * `empty("00")` was true and `array_filter(["00"])` dropped it, where php`` |
|        - | 1408 | ``	 * keeps both (only `""` and the single byte `"0"` are false there). The`` |
|        - | 1409 | `	 * warning php raises when a NaN is coerced rides the same door, since` |
|        - | 1410 | ``	 * `empty(NAN)` warns there. */`` |
|    55971 | 1411 | `	if( (pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_REAL)) == MEMOBJ_REAL` |
|    27985 | 1412 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        5 | 1413 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        - | 1414 | `			"unexpected NAN value was coerced to bool");` |
|        2 | 1415 | `	}` |
|    55976 | 1416 | `	return !MemObjIsTruthy(&(*pObj));` |
|        5 | 1417 | `}` |
|        - | 1418 | `/*` |
|        - | 1419 | ` * Convert a ph7_value so that it has types MEMOBJ_REAL or MEMOBJ_INT` |
|        - | 1420 | ` * or both.` |
|        - | 1421 | ` * Invalidate any prior representations. Every effort is made to force` |
|        - | 1422 | ` * the conversion, even if the input is a string that does not look` |
|        - | 1423 | ` * completely like a number.Convert as much of the string as we can` |
|        - | 1424 | ` * and ignore the rest.` |
|        - | 1425 | ` */` |
|  2057996 | 1426 | `PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj)` |
|        5 | 1427 | `{` |
|  2058001 | 1428 | `	if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|  2055981 | 1429 | `		if( pObj->iFlags & (MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|     1436 | 1430 | `			if( pObj->iFlags & MEMOBJ_NULL ){` |
|      537 | 1431 | `				pObj->x.iVal = 0;` |
|      267 | 1432 | `			}` |
|     1436 | 1433 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|      716 | 1434 | `		}` |
|        - | 1435 | `		/* Already numeric */` |
|  2055981 | 1436 | `		return  SXRET_OK;` |
|        - | 1437 | `	}` |
|     2025 | 1438 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     2021 | 1439 | `		const char *zTail = 0;` |
|     2021 | 1440 | `		int bNum, bReal = 0;` |
|        - | 1441 | `		/* php reads the longest numeric PREFIX and its shape decides the type: a` |
|        - | 1442 | `		 * '.' or a *complete* exponent inside that prefix makes it a float, else an` |
|        - | 1443 | `		 * int. Deciding from the raw string instead mistyped "1e" as float(1) --` |
|        - | 1444 | `		 * php sees the prefix "1" there and yields int(1). */` |
|     2021 | 1445 | `		bNum = PH7_MemObjStringNumericPrefix(pObj,&zTail);` |
|     2021 | 1446 | `		if( bNum ){` |
|     1985 | 1447 | `			const char *z = (const char *)SyBlobData(&pObj->sBlob);` |
|     9271 | 1448 | `			while( z < zTail ){` |
|     7479 | 1449 | `				if( z[0] == '.' \|\| z[0] == 'e' \|\| z[0] == 'E' ){` |
|      192 | 1450 | `					bReal = 1;` |
|      192 | 1451 | `					break;` |
|        - | 1452 | `				}` |
|     7291 | 1453 | `				z++;` |
|        5 | 1454 | `			}` |
|      986 | 1455 | `		}` |
|     2021 | 1456 | `		if( bReal ){` |
|      192 | 1457 | `			PH7_MemObjToReal(&(*pObj));` |
|       98 | 1458 | `		}else{` |
|     1833 | 1459 | `			if( !bNum ){` |
|        - | 1460 | `				/* The input does not look at all like a number,set the value to 0 */` |
|       37 | 1461 | `				pObj->x.iVal = 0;` |
|       19 | 1462 | `			}else{` |
|     1797 | 1463 | `				int iOverflow = 0;` |
|        - | 1464 | `				/* Convert as much as we can */` |
|     1797 | 1465 | `				pObj->x.iVal = MemObjStringToInt(&(*pObj),&iOverflow);` |
|        - | 1466 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     1797 | 1467 | `				if( iOverflow ){` |
|        - | 1468 | `					/* php: an integer-shaped numeric string whose digit run runs past` |
|        - | 1469 | `					 * the int64 range is a FLOAT, and every arithmetic operator` |
|        - | 1470 | `					 * inherits that because they all come through here. Clamping it` |
|        - | 1471 | `					 * instead answered PHP_INT_MAX for "9223372036854775808" + 0 and` |
|        - | 1472 | `					 * -- worse -- PHP_INT_MIN for "-9223372036854775809" + 0, a value` |
|        - | 1473 | `					 * with no relation to the input. The float is read from the same` |
|        - | 1474 | `					 * bytes by MemObjRealValue's SyStrToReal, which is also what the` |
|        - | 1475 | `					 * (float) cast has always answered; the (int) CAST keeps` |
|        - | 1476 | `					 * saturating, as php's does. The integer-only build has no float` |
|        - | 1477 | `					 * to promote TO, so it keeps the saturated int -- the same choice` |
|        - | 1478 | `					 * OP_ADD's overflow arm makes there. */` |
|      228 | 1479 | `					PH7_MemObjToReal(&(*pObj));` |
|      228 | 1480 | `					return SXRET_OK;` |
|        - | 1481 | `				}` |
|        - | 1482 | `#endif` |
|        - | 1483 | `			}` |
|     1607 | 1484 | `			MemObjSetType(pObj,MEMOBJ_INT);` |
|     1607 | 1485 | `			SyBlobRelease(&pObj->sBlob);` |
|        5 | 1486 | `		}` |
|      896 | 1487 | `	}else if(pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)){` |
|        5 | 1488 | `		if( pObj->iFlags & MEMOBJ_OBJ ){` |
|        5 | 1489 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|        4 | 1490 | `			if( pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass)` |
|        5 | 1491 | `			 && PH7_MemObjToString(pObj) == SXRET_OK ){` |
|        - | 1492 | `				/* php's cast_object answers IS_NUMBER from the object's TEXT, and` |
|        - | 1493 | `				 * the text's own shape then decides int or float -- so` |
|        - | 1494 | ``				 * `$xml->price + 0` on `<price>2.5</price>` is 2.5 and not 2. The`` |
|        - | 1495 | `				 * value is a STRING now, so this recursion ends here. */` |
|        5 | 1496 | `				return PH7_MemObjToNumeric(pObj);` |
|        - | 1497 | `			}` |
|      ! 0 | 1498 | `		}` |
|      ! 0 | 1499 | `		PH7_MemObjToInteger(pObj);` |
|      ! 0 | 1500 | `	}else{` |
|        - | 1501 | `		/* Perform a blind cast */` |
|      ! 0 | 1502 | `		PH7_MemObjToReal(&(*pObj));` |
|        - | 1503 | `	}` |
|     1795 | 1504 | `	return SXRET_OK;` |
|  1030500 | 1505 | `}` |
|        - | 1506 | `/*` |
|        - | 1507 | ` * Apply Perl-style increment to a string ph7_value in place.` |
|        - | 1508 | ` * Walks the bytes right-to-left: digits 0-8 / letters a-y, A-Y bump in` |
|        - | 1509 | ` * place; '9' wraps to '0' with carry; 'z' to 'a'; 'Z' to 'A'. A non-` |
|        - | 1510 | ` * alphanumeric byte stops the walk without prepending. If carry survives` |
|        - | 1511 | ` * past index 0, prepend '1', 'a', or 'A' depending on the class of the` |
|        - | 1512 | ` * last carried character. Empty strings become "1".` |
|        - | 1513 | ` *` |
|        - | 1514 | ` * Caller must ensure pObj is MEMOBJ_STRING and not a numeric string;` |
|        - | 1515 | ` * this routine never reclassifies the type, so a result like "e0" stays` |
|        - | 1516 | ` * a string even though it looks numeric.` |
|        - | 1517 | ` */` |
|      ! 0 | 1518 | `PH7_PRIVATE sxi32 PH7_MemObjStringIncrement(ph7_value *pObj)` |
|      ! 0 | 1519 | `{` |
|        - | 1520 | `	enum CarryClass { CARRY_NONE = 0, CARRY_LOWER, CARRY_UPPER, CARRY_DIGIT };` |
|      ! 0 | 1521 | `	enum CarryClass last_class = CARRY_NONE;` |
|        - | 1522 | `	sxu32 nLen, pos;` |
|        - | 1523 | `	sxu8 *zStr;` |
|      ! 0 | 1524 | `	int carry = 1;` |
|        - | 1525 | `	int ch;` |
|        - | 1526 | `	/* Force ownership: the blob may be SXBLOB_RDONLY (e.g., from` |
|        - | 1527 | `	 * PH7_MemObjLoad), in which case BlobPrepareGrow copies on demand` |
|        - | 1528 | `	 * and clears the flag.  On an already-owned blob with spare capacity` |
|        - | 1529 | `	 * (the common case under PHL's growth allocator), this is a no-op` |
|        - | 1530 | `	 * append; on an exact-fit owned blob it triggers a single realloc. */` |
|      ! 0 | 1531 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      ! 0 | 1532 | `		SyBlobNullAppend(&pObj->sBlob);` |
|      ! 0 | 1533 | `	}` |
|      ! 0 | 1534 | `	nLen = SyBlobLength(&pObj->sBlob);` |
|      ! 0 | 1535 | `	if( nLen == 0 ){` |
|      ! 0 | 1536 | `		SyBlobAppend(&pObj->sBlob,"1",sizeof(char));` |
|      ! 0 | 1537 | `		return SXRET_OK;` |
|        - | 1538 | `	}` |
|      ! 0 | 1539 | `	zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1540 | `	pos = nLen;` |
|      ! 0 | 1541 | `	while( pos > 0 ){` |
|      ! 0 | 1542 | `		pos--;` |
|      ! 0 | 1543 | `		ch = zStr[pos];` |
|      ! 0 | 1544 | `		if( ch >= 'a' && ch <= 'z' ){` |
|      ! 0 | 1545 | `			if( ch == 'z' ){` |
|      ! 0 | 1546 | `				zStr[pos] = 'a';` |
|      ! 0 | 1547 | `				last_class = CARRY_LOWER;` |
|      ! 0 | 1548 | `				continue;` |
|        - | 1549 | `			}` |
|      ! 0 | 1550 | `			zStr[pos]++;` |
|      ! 0 | 1551 | `			carry = 0;` |
|      ! 0 | 1552 | `			break;` |
|      ! 0 | 1553 | `		}else if( ch >= 'A' && ch <= 'Z' ){` |
|      ! 0 | 1554 | `			if( ch == 'Z' ){` |
|      ! 0 | 1555 | `				zStr[pos] = 'A';` |
|      ! 0 | 1556 | `				last_class = CARRY_UPPER;` |
|      ! 0 | 1557 | `				continue;` |
|        - | 1558 | `			}` |
|      ! 0 | 1559 | `			zStr[pos]++;` |
|      ! 0 | 1560 | `			carry = 0;` |
|      ! 0 | 1561 | `			break;` |
|      ! 0 | 1562 | `		}else if( ch >= '0' && ch <= '9' ){` |
|      ! 0 | 1563 | `			if( ch == '9' ){` |
|      ! 0 | 1564 | `				zStr[pos] = '0';` |
|      ! 0 | 1565 | `				last_class = CARRY_DIGIT;` |
|      ! 0 | 1566 | `				continue;` |
|        - | 1567 | `			}` |
|      ! 0 | 1568 | `			zStr[pos]++;` |
|      ! 0 | 1569 | `			carry = 0;` |
|      ! 0 | 1570 | `			break;` |
|      ! 0 | 1571 | `		}else{` |
|        - | 1572 | `			/* non-alphanumeric: stop without prepending */` |
|      ! 0 | 1573 | `			carry = 0;` |
|      ! 0 | 1574 | `			break;` |
|        - | 1575 | `		}` |
|      ! 0 | 1576 | `	}` |
|      ! 0 | 1577 | `	if( carry ){` |
|        - | 1578 | `		sxu8 prepend;` |
|        - | 1579 | `		sxu32 i;` |
|      ! 0 | 1580 | `		switch( last_class ){` |
|      ! 0 | 1581 | `			case CARRY_LOWER: prepend = (sxu8)'a'; break;` |
|      ! 0 | 1582 | `			case CARRY_UPPER: prepend = (sxu8)'A'; break;` |
|      ! 0 | 1583 | `			default:          prepend = (sxu8)'1'; break;` |
|        - | 1584 | `		}` |
|        - | 1585 | `		/* Append a sentinel byte to grow nByte by 1 (capacity grows too). */` |
|      ! 0 | 1586 | `		SyBlobAppend(&pObj->sBlob,"\0",sizeof(char));` |
|      ! 0 | 1587 | `		zStr = (sxu8 *)SyBlobData(&pObj->sBlob);` |
|      ! 0 | 1588 | `		nLen = SyBlobLength(&pObj->sBlob);` |
|        - | 1589 | `		/* Shift right by 1, walking from the end so overlapping is safe. */` |
|      ! 0 | 1590 | `		for( i = nLen - 1; i > 0; i-- ){` |
|      ! 0 | 1591 | `			zStr[i] = zStr[i - 1];` |
|      ! 0 | 1592 | `		}` |
|      ! 0 | 1593 | `		zStr[0] = prepend;` |
|      ! 0 | 1594 | `	}` |
|      ! 0 | 1595 | `	return SXRET_OK;` |
|      ! 0 | 1596 | `}` |
|        - | 1597 | `/*` |
|        - | 1598 | ` * Try a get an integer representation of the given ph7_value.` |
|        - | 1599 | ` * If the ph7_value is not of type real,this function is a no-op.` |
|        - | 1600 | ` */` |
|     3755 | 1601 | `PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj)` |
|        5 | 1602 | `{` |
|     3760 | 1603 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 1604 | `		/* Work only with reals */` |
|     3760 | 1605 | `		MemObjTryIntger(&(*pObj));` |
|     1867 | 1606 | `	}` |
|     3760 | 1607 | `	return SXRET_OK;` |
|        5 | 1608 | `}` |
|        - | 1609 | `/*` |
|        - | 1610 | ` * Initialize a ph7_value to the null type.` |
|        - | 1611 | ` */` |
| 42688455 | 1612 | `PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj)` |
|        5 | 1613 | `{` |
|        - | 1614 | `	PHL_VC_NOTE(PHL_VC_INIT,1);` |
|        - | 1615 | `	/* Zero the structure */` |
| 42688460 | 1616 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1617 | `	/* Initialize fields */` |
| 42688460 | 1618 | `	pObj->pVm = pVm;` |
| 42688460 | 1619 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1620 | `	/* Set the NULL type */` |
| 42688460 | 1621 | `	pObj->iFlags = MEMOBJ_NULL;` |
| 42688460 | 1622 | `	return SXRET_OK;` |
|        5 | 1623 | `}` |
|        - | 1624 | `/*` |
|        - | 1625 | ` * Initialize a ph7_value to the integer type.` |
|        - | 1626 | ` */` |
|  8072142 | 1627 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal)` |
|        5 | 1628 | `{` |
|        - | 1629 | `	/* Zero the structure */` |
|  8072147 | 1630 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1631 | `	/* Initialize fields */` |
|  8072147 | 1632 | `	pObj->pVm = pVm;` |
|  8072147 | 1633 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1634 | `	/* Set the desired type */` |
|  8072147 | 1635 | `	pObj->x.iVal = iVal;` |
|  8072147 | 1636 | `	pObj->iFlags = MEMOBJ_INT;` |
|  8072147 | 1637 | `	return SXRET_OK;` |
|        5 | 1638 | `}` |
|        - | 1639 | `/*` |
|        - | 1640 | ` * Initialize a ph7_value to the boolean type.` |
|        - | 1641 | ` */` |
|    21190 | 1642 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal)` |
|        5 | 1643 | `{` |
|        - | 1644 | `	/* Zero the structure */` |
|    21195 | 1645 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1646 | `	/* Initialize fields */` |
|    21195 | 1647 | `	pObj->pVm = pVm;` |
|    21195 | 1648 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1649 | `	/* Set the desired type */` |
|    21195 | 1650 | `	pObj->x.iVal = iVal ? 1 : 0;` |
|    21195 | 1651 | `	pObj->iFlags = MEMOBJ_BOOL;` |
|    21195 | 1652 | `	return SXRET_OK;` |
|        5 | 1653 | `}` |
|        - | 1654 | `/*` |
|        - | 1655 | ` * Initialize a ph7_value to the real type.` |
|        - | 1656 | ` */` |
|     1454 | 1657 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal)` |
|        3 | 1658 | `{` |
|        - | 1659 | `	/* Zero the structure */` |
|     1457 | 1660 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1661 | `	/* Initialize fields */` |
|     1457 | 1662 | `	pObj->pVm = pVm;` |
|     1457 | 1663 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1664 | `	/* Set the desired type */` |
|     1457 | 1665 | `	pObj->rVal = rVal;` |
|     1457 | 1666 | `	pObj->iFlags = MEMOBJ_REAL;` |
|     1457 | 1667 | `	return SXRET_OK;` |
|        3 | 1668 | `}` |
|        - | 1669 | `/*` |
|        - | 1670 | ` * Initialize a ph7_value to the array type.` |
|        - | 1671 | ` */` |
|  4589757 | 1672 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray)` |
|        5 | 1673 | `{` |
|        - | 1674 | `	/* Zero the structure */` |
|  4589762 | 1675 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1676 | `	/* Initialize fields */` |
|  4589762 | 1677 | `	pObj->pVm = pVm;` |
|  4589762 | 1678 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
|        - | 1679 | `	/* Set the desired type */` |
|  4589762 | 1680 | `	pObj->iFlags = MEMOBJ_HASHMAP;` |
|  4589762 | 1681 | `	pObj->x.pOther = pArray;` |
|  4589762 | 1682 | `	return SXRET_OK;` |
|        5 | 1683 | `}` |
|        - | 1684 | `/*` |
|        - | 1685 | ` * Initialize a ph7_value to the string type.` |
|        - | 1686 | ` */` |
| 12418927 | 1687 | `PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal)` |
|        5 | 1688 | `{` |
|        - | 1689 | `	/* Zero the structure */` |
| 12418932 | 1690 | `	SyZero(pObj,sizeof(ph7_value));` |
|        - | 1691 | `	/* Initialize fields */` |
| 12418932 | 1692 | `	pObj->pVm = pVm;` |
| 12418932 | 1693 | `	SyBlobInit(&pObj->sBlob,&pVm->sAllocator);` |
| 12418932 | 1694 | `	if( pVal ){` |
|        - | 1695 | `		/* Append contents */` |
|  8432905 | 1696 | `		SyBlobAppend(&pObj->sBlob,(const void *)pVal->zString,pVal->nByte);` |
|  4215709 | 1697 | `	}` |
|        - | 1698 | `	/* Set the desired type */` |
| 12418932 | 1699 | `	pObj->iFlags = MEMOBJ_STRING;` |
| 12418932 | 1700 | `	return SXRET_OK;` |
|        5 | 1701 | `}` |
|        - | 1702 | `/*` |
|        - | 1703 | ` * Append some contents to the internal buffer of a given ph7_value.` |
|        - | 1704 | ` * If the given ph7_value is not of type string,this function` |
|        - | 1705 | ` * invalidate any prior representation and set the string type.` |
|        - | 1706 | ` * Then a simple append operation is performed.` |
|        - | 1707 | ` */` |
|  4908737 | 1708 | `PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen)` |
|        5 | 1709 | `{` |
|        - | 1710 | `	sxi32 rc;` |
|  4908742 | 1711 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1712 | `		/* Invalidate any prior representation */` |
|    76031 | 1713 | `		PH7_MemObjRelease(pObj);` |
|    76031 | 1714 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|    37654 | 1715 | `	}` |
|        - | 1716 | `	/* Append contents */` |
|  4908742 | 1717 | `	rc = SyBlobAppend(&pObj->sBlob,zData,nLen);` |
|  4908742 | 1718 | `	return rc;` |
|        5 | 1719 | `}` |
|        - | 1720 | `#if 0` |
|        - | 1721 | `/*` |
|        - | 1722 | ` * Format and append some contents to the internal buffer of a given ph7_value.` |
|        - | 1723 | ` * If the given ph7_value is not of type string,this function invalidate` |
|        - | 1724 | ` * any prior representation and set the string type.` |
|        - | 1725 | ` * Then a simple format and append operation is performed.` |
|        - | 1726 | ` */` |
|        - | 1727 | `PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap)` |
|        - | 1728 | `{` |
|        - | 1729 | `	sxi32 rc;` |
|        - | 1730 | `	if( (pObj->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - | 1731 | `		/* Invalidate any prior representation */` |
|        - | 1732 | `		PH7_MemObjRelease(pObj);` |
|        - | 1733 | `		MemObjSetType(pObj,MEMOBJ_STRING);` |
|        - | 1734 | `	}` |
|        - | 1735 | `	/* Format and append contents */` |
|        - | 1736 | `	rc = SyBlobFormatAp(&pObj->sBlob,zFormat,ap);` |
|        - | 1737 | `	return rc;` |
|        - | 1738 | `}` |
|        - | 1739 | `#endif` |
|        - | 1740 | `/*` |
|        - | 1741 | ` * Duplicate the contents of a ph7_value.` |
|        - | 1742 | ` */` |
| 31008187 | 1743 | `PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest)` |
|        5 | 1744 | `{` |
| 31008192 | 1745 | `	ph7_class_instance *pObj = 0;` |
| 31008192 | 1746 | `	ph7_hashmap *pMap = 0;` |
|        - | 1747 | `	sxi32 rc;` |
|        - | 1748 | `	PHL_VC_NOTE(PHL_VC_STORE,(pSrc->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0);` |
| 31008192 | 1749 | `	if( pSrc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 1750 | `		/* Increment reference count */` |
|  4199728 | 1751 | `		((ph7_hashmap *)pSrc->x.pOther)->iRef++;` |
| 28908016 | 1752 | `	}else if( pSrc->iFlags & MEMOBJ_OBJ ){` |
|        - | 1753 | `		/* Increment reference count */` |
|   122822 | 1754 | `		((ph7_class_instance *)pSrc->x.pOther)->iRef++;` |
|    61229 | 1755 | `	}` |
| 31008192 | 1756 | `	if( pDest->iFlags & MEMOBJ_HASHMAP ){` |
|   206771 | 1757 | `		pMap = (ph7_hashmap *)pDest->x.pOther;` |
| 30904749 | 1758 | `	}else if( pDest->iFlags & MEMOBJ_OBJ ){` |
|    72415 | 1759 | `		pObj = (ph7_class_instance *)pDest->x.pOther;` |
|    36180 | 1760 | `	}` |
| 31008192 | 1761 | `	PH7_MEMOBJ_COPY_SCALAR(pDest,pSrc);` |
| 31008192 | 1762 | `	pDest->iFlags &= ~MEMOBJ_AUX;` |
| 31008192 | 1763 | `	rc = SXRET_OK;` |
| 31008192 | 1764 | `	if( SyBlobLength(&pSrc->sBlob) > 0 ){` |
| 15135532 | 1765 | `		SyBlobReset(&pDest->sBlob);` |
| 15135532 | 1766 | `		rc = SyBlobDup(&pSrc->sBlob,&pDest->sBlob);` |
|  7562340 | 1767 | `	}else{` |
| 15872665 | 1768 | `		if( SyBlobLength(&pDest->sBlob) > 0 ){` |
|  2968197 | 1769 | `			SyBlobRelease(&pDest->sBlob);` |
|  1484650 | 1770 | `		}` |
|        - | 1771 | `	}` |
| 31008192 | 1772 | `	if( pMap ){` |
|   206771 | 1773 | `		PH7_HashmapUnref(pMap);` |
| 30904749 | 1774 | `	}else if( pObj ){` |
|    72415 | 1775 | `		PH7_ClassInstanceUnref(pObj);` |
|    36180 | 1776 | `	}` |
| 31008187 | 1777 | `	if( rc == SXRET_OK && (pDest->iFlags & MEMOBJ_HASHMAP)` |
| 17597593 | 1778 | `	 && pDest->pVm` |
|  4199723 | 1779 | `	 && (ph7_hashmap *)pDest->x.pOther == pDest->pVm->pGlobal` |
|        - | 1780 | `	 /* Identity, not nIdx: transient values carry nIdx==0 (SyZero), which` |
|        - | 1781 | `	  * collides with a typical nGlobalIdx of 0 and would skip the snapshot` |
|        - | 1782 | `	  * for closure envs and other non-slot destinations. */` |
|  2099556 | 1783 | `	 && pDest != (ph7_value *)PH7_MemObjAt(&pDest->pVm->aMemObj,pDest->pVm->nGlobalIdx) ){` |
|        - | 1784 | `		/* php 8.1: a COPY of $GLOBALS ($snap = $GLOBALS, $a[] = $GLOBALS,` |
|        - | 1785 | `		 * by-value argument passing, return $GLOBALS, ...) is a by-value` |
|        - | 1786 | `		 * SNAPSHOT of the symbol table with its reference entries` |
|        - | 1787 | `		 * flattened — never a live alias. Materialize it here, the one` |
|        - | 1788 | `		 * store choke point (loads/subscript access keep sharing, so` |
|        - | 1789 | `		 * $GLOBALS[$k] reads and writes stay live). */` |
|        9 | 1790 | `		ph7_hashmap *pSnap = PH7_NewHashmap(pDest->pVm,0,0);` |
|        9 | 1791 | `		if( pSnap && PH7_HashmapDupMaterialized((ph7_hashmap *)pDest->x.pOther,pSnap) == SXRET_OK ){` |
|        9 | 1792 | `			PH7_HashmapUnref((ph7_hashmap *)pDest->x.pOther);` |
|        9 | 1793 | `			pDest->x.pOther = pSnap;` |
|        4 | 1794 | `		}else if( pSnap ){` |
|      ! 0 | 1795 | `			PH7_HashmapUnref(pSnap);` |
|      ! 0 | 1796 | `		}` |
|        4 | 1797 | `	}` |
| 31008192 | 1798 | `	return rc;` |
|        5 | 1799 | `}` |
|        - | 1800 | `/*` |
|        - | 1801 | ` * Read a value WITHOUT converting the caller's copy of it.` |
|        - | 1802 | ` *` |
|        - | 1803 | ` * Every ph7_value_to_xxx()/PH7_MemObjToXxx() is destructive: it rewrites the` |
|        - | 1804 | ` * object it is handed and throws the prior representation away. That is right` |
|        - | 1805 | ` * for a VM operand, and wrong for an entry a builtin FETCHED out of an array` |
|        - | 1806 | ` * the script still holds — an $options member, a stream-filter parameter, a` |
|        - | 1807 | ` * proc_open descriptor — where converting in place rewrites the script's own` |
|        - | 1808 | ` * array (php's zval_get_long()/zval_get_string() family never touch theirs).` |
|        - | 1809 | ` *` |
|        - | 1810 | ` * PH7_ValuePeek loads an aliasing copy into pScratch (which the caller must` |
|        - | 1811 | ` * have PH7_MemObjInit'd and must PH7_MemObjRelease afterwards) and answers it,` |
|        - | 1812 | ` * so the destructive conversion lands on the copy. A string read through it` |
|        - | 1813 | ` * stays valid until the scratch value is released. The three scalar wrappers` |
|        - | 1814 | ` * carry their own scratch for the common case.` |
|        - | 1815 | ` */` |
|     1450 | 1816 | `PH7_PRIVATE ph7_value * PH7_ValuePeek(ph7_value *pVal,ph7_value *pScratch)` |
|        3 | 1817 | `{` |
|     1453 | 1818 | `	PH7_MemObjLoad(pVal,pScratch);` |
|     1453 | 1819 | `	return pScratch;` |
|        3 | 1820 | `}` |
|     1746 | 1821 | `PH7_PRIVATE sxi64 PH7_ValuePeekInt64(ph7_value *pVal)` |
|        5 | 1822 | `{` |
|        - | 1823 | `	ph7_value sTmp;` |
|        - | 1824 | `	sxi64 iVal;` |
|     1751 | 1825 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|     1751 | 1826 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|     1751 | 1827 | `	PH7_MemObjToInteger(&sTmp);` |
|     1751 | 1828 | `	iVal = sTmp.x.iVal;` |
|     1751 | 1829 | `	PH7_MemObjRelease(&sTmp);` |
|     1751 | 1830 | `	return iVal;` |
|        5 | 1831 | `}` |
|        - | 1832 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      524 | 1833 | `PH7_PRIVATE ph7_real PH7_ValuePeekReal(ph7_value *pVal)` |
|        3 | 1834 | `{` |
|        - | 1835 | `	ph7_value sTmp;` |
|        - | 1836 | `	ph7_real rVal;` |
|      527 | 1837 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|      527 | 1838 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|      527 | 1839 | `	PH7_MemObjToReal(&sTmp);` |
|      527 | 1840 | `	rVal = sTmp.rVal;` |
|      527 | 1841 | `	PH7_MemObjRelease(&sTmp);` |
|      527 | 1842 | `	return rVal;` |
|        3 | 1843 | `}` |
|        - | 1844 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|        6 | 1845 | `PH7_PRIVATE int PH7_ValuePeekBool(ph7_value *pVal)` |
|        2 | 1846 | `{` |
|        - | 1847 | `	ph7_value sTmp;` |
|        - | 1848 | `	int bVal;` |
|        8 | 1849 | `	PH7_MemObjInit(pVal->pVm,&sTmp);` |
|        8 | 1850 | `	PH7_MemObjLoad(pVal,&sTmp);` |
|        8 | 1851 | `	PH7_MemObjToBool(&sTmp);` |
|        8 | 1852 | `	bVal = sTmp.x.iVal != 0;` |
|        8 | 1853 | `	PH7_MemObjRelease(&sTmp);` |
|        8 | 1854 | `	return bVal;` |
|        2 | 1855 | `}` |
|        - | 1856 | `/*` |
|        - | 1857 | ` * Invalidate any prior representation of a given ph7_value.` |
|        - | 1858 | ` *` |
|        - | 1859 | ` * The SLOW half of PH7_MemObjRelease (ph7int.h), which is the door every caller` |
|        - | 1860 | ` * still writes. This body runs only for a value that actually owns something: a` |
|        - | 1861 | ` * hashmap or instance reference, a string blob, or one of the three AUX carriers.` |
|        - | 1862 | ` * The inline guard turns the other 43.1% of the engine's 2.72 billion releases` |
|        - | 1863 | ` * into a test. Nothing below may act on a value that is MEMOBJ_NULL and carries` |
|        - | 1864 | ` * no AUX bit -- that combination never reaches here.` |
|        - | 1865 | ` */` |
| 92123248 | 1866 | `PH7_PRIVATE sxi32 PH7_MemObjReleaseSlow(ph7_value *pObj)` |
|        5 | 1867 | `{` |
| 92123253 | 1868 | `	if( pObj->iFlags & MEMOBJ_AUX_COALSTROFF ){` |
|        - | 1869 | ``		/* A `$s[k] ??= v` peek result OWNS the heap VmCoalStrOff holding its raw`` |
|        - | 1870 | `		 * offset. Free it HERE, before the MEMOBJ_NULL short-circuit below and for` |
|        - | 1871 | `		 * the same reason as the DEFPATH carrier above: this is the universal` |
|        - | 1872 | `		 * release site every pop / abort / exception-unwind routes through, so an` |
|        - | 1873 | ``		 * abandoned `??=` cannot leak the offset. */`` |
|        7 | 1874 | `		VmFreeCoalStrOff((VmCoalStrOff *)pObj->x.pOther);` |
|        7 | 1875 | `		pObj->x.pOther = 0;` |
|        7 | 1876 | `		pObj->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|        3 | 1877 | `	}` |
| 92123253 | 1878 | `	if( pObj->iFlags & MEMOBJ_AUX_MAGICCALL ){` |
|        - | 1879 | `		/* A __call/__callStatic carrier OWNS the heap VmMagicCall holding its receiver` |
|        - | 1880 | `		 * reference, class and original name. Freed HERE for the same reason as the two` |
|        - | 1881 | `		 * carriers below: this is the universal release site, so a routed call whose` |
|        - | 1882 | `		 * argument list threw never leaks the receiver it was holding. */` |
|      ! 0 | 1883 | `		VmFreeMagicCall((VmMagicCall *)pObj->x.pOther);` |
|      ! 0 | 1884 | `		pObj->x.pOther = 0;` |
|      ! 0 | 1885 | `		pObj->iFlags &= ~MEMOBJ_AUX_MAGICCALL;` |
|      ! 0 | 1886 | `	}` |
| 92123253 | 1887 | `	if( pObj->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|        - | 1888 | `		/* D1 commit 2: a deferred element/property lvalue carrier OWNS a heap VmDeferredPath` |
|        - | 1889 | `		 * on a NULL-typed slot. Free it HERE, before the MEMOBJ_NULL short-circuit below —` |
|        - | 1890 | `		 * this is the universal release site every pop / abort / exception-unwind path routes` |
|        - | 1891 | `		 * through, so the descriptor never leaks even when OP_CALL never consumes it. */` |
|        3 | 1892 | `		VmFreeDeferredPath((VmDeferredPath *)pObj->x.pOther);` |
|        3 | 1893 | `		pObj->x.pOther = 0;` |
|        3 | 1894 | `		pObj->iFlags &= ~MEMOBJ_AUX_DEFPATH;` |
|        1 | 1895 | `	}` |
| 92123253 | 1896 | `	if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
| 92123245 | 1897 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|  9564572 | 1898 | `			PH7_HashmapUnref((ph7_hashmap *)pObj->x.pOther);` |
| 87340185 | 1899 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|  5223909 | 1900 | `			PH7_ClassInstanceUnref((ph7_class_instance *)pObj->x.pOther);` |
|  2611381 | 1901 | `		}` |
|        - | 1902 | `		/* Release the internal buffer */` |
| 92123245 | 1903 | `		SyBlobRelease(&pObj->sBlob);` |
|        - | 1904 | `		/* Invalidate any prior representation */` |
| 92123245 | 1905 | `		pObj->iFlags = MEMOBJ_NULL;` |
| 46058335 | 1906 | `	}` |
| 92123253 | 1907 | `	return SXRET_OK;` |
|        5 | 1908 | `}` |
|        - | 1909 | `/*` |
|        - | 1910 | ` * php's object-vs-scalar comparison cast: the default arm of zend_compare hands` |
|        - | 1911 | ` * the object to its class's cast_object handler with the OTHER operand's type,` |
|        - | 1912 | ` * and compares the result. Build that cast of pSelf in *pOut and answer TRUE;` |
|        - | 1913 | ` * answer FALSE when php's std handler refuses the conversion, in which case the` |
|        - | 1914 | ` * caller reports the object as greater, exactly as php does.` |
|        - | 1915 | ` *` |
|        - | 1916 | ` * The refusals are: a STRING target with no __toString(), and any null / array /` |
|        - | 1917 | ` * resource target (php's handler only knows string, bool, int and float). *pOut` |
|        - | 1918 | ` * is always initialized, so the caller can release it either way.` |
|        - | 1919 | ` *` |
|        - | 1920 | ` * The int and float targets never fail — the object becomes 1 / 1.0 — but they` |
|        - | 1921 | `` * do diagnose, and at E_NOTICE, where the `(int)`/`(float)` CASTS raise`` |
|        - | 1922 | ` * E_WARNING from MemObjIntValue/MemObjRealValue. php raises the two from` |
|        - | 1923 | ` * different places with different severities, so this one is emitted here rather` |
|        - | 1924 | ` * than borrowed from the cast helpers. It names the OTHER operand's type, so` |
|        - | 1925 | `` * `$o <=> 20.0` says "float" even though 20.0 is an integral value (which in PHL`` |
|        - | 1926 | ` * carries MEMOBJ_INT alongside MEMOBJ_REAL — hence testing REAL first).` |
|        - | 1927 | ` */` |
|      230 | 1928 | `static int MemObjCmpCastObject(ph7_value *pSelf,ph7_value *pOther,ph7_value *pOut)` |
|        4 | 1929 | `{` |
|      234 | 1930 | `	ph7_class_instance *pInst = (ph7_class_instance *)pSelf->x.pOther;` |
|      234 | 1931 | `	PH7_MemObjInit(pSelf->pVm,pOut);` |
|      234 | 1932 | `	if( pOther->iFlags & MEMOBJ_STRING ){` |
|      152 | 1933 | `		if( PH7_MemObjIsNotStringable(pSelf)` |
|      143 | 1934 | `		 \|\| (pSelf->pVm && PH7_CALLBACK_UNWOUND(pSelf->pVm->nBoundaryRc)) ){` |
|        - | 1935 | `			/* php enters no PHP function while an exception is pending` |
|        - | 1936 | `			 * (zend_call_function bails on EG(exception)), so a __toString()` |
|        - | 1937 | `			 * that already threw -- or exited -- is NOT run again: every later` |
|        - | 1938 | `			 * comparison orders this operand the way a refused cast does. A sort` |
|        - | 1939 | `			 * used to re-enter the body once per remaining pair, and where the` |
|        - | 1940 | `			 * enclosing catch had already run in place the second throw was` |
|        - | 1941 | `			 * UNCAUGHT and killed the script. */` |
|       63 | 1942 | `			return FALSE;` |
|        - | 1943 | `		}` |
|       95 | 1944 | `		PH7_MemObjLoad(pSelf,pOut);` |
|       95 | 1945 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|        - | 1946 | `			/* __toString() threw. The throw is parked and lands at the next fetch` |
|        - | 1947 | `			 * point; until then order the operands the way a refused cast does. */` |
|       39 | 1948 | `			return FALSE;` |
|        - | 1949 | `		}` |
|       56 | 1950 | `		return TRUE;` |
|        - | 1951 | `	}` |
|       81 | 1952 | `	if( pOther->iFlags & MEMOBJ_BOOL ){` |
|        - | 1953 | `		/* An object is always truthy, with no diagnostic (php has no __toBool). */` |
|       25 | 1954 | `		PH7_MemObjInitFromBool(pSelf->pVm,pOut,1);` |
|       25 | 1955 | `		return TRUE;` |
|        - | 1956 | `	}` |
|       56 | 1957 | `	if( (pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL))` |
|       43 | 1958 | `	 && pInst && pInst->pClass && PH7_ClassNumberIsString(pInst->pClass) ){` |
|        - | 1959 | `		/* A class whose cast_object really answers a number: php compares against` |
|        - | 1960 | ``		 * THAT, silently -- `(string)$xml->n == 5` and `$xml->n == 5` agree. */`` |
|        5 | 1961 | `		PH7_MemObjLoad(pSelf,pOut);` |
|        5 | 1962 | `		if( PH7_MemObjToString(pOut) != SXRET_OK ){` |
|      ! 0 | 1963 | `			return FALSE;` |
|        - | 1964 | `		}` |
|        5 | 1965 | `		PH7_MemObjToNumeric(pOut);` |
|        5 | 1966 | `		return TRUE;` |
|        - | 1967 | `	}` |
|       54 | 1968 | `	if( pOther->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|       21 | 1969 | `		int bReal = (pOther->iFlags & MEMOBJ_REAL) != 0;` |
|       21 | 1970 | `		if( pInst && pInst->pClass && pSelf->pVm ){` |
|       31 | 1971 | `			VmErrorFormat(pSelf->pVm,PH7_CTX_NOTICE,` |
|        - | 1972 | `				"Object of class %z could not be converted to %s",` |
|       20 | 1973 | `				&pInst->pClass->sName,bReal ? "float" : "int");` |
|       10 | 1974 | `		}` |
|       21 | 1975 | `		if( bReal ){` |
|        7 | 1976 | `			PH7_MemObjInitFromReal(pSelf->pVm,pOut,(ph7_real)1.0);` |
|        4 | 1977 | `		}else{` |
|       15 | 1978 | `			PH7_MemObjInitFromInt(pSelf->pVm,pOut,1);` |
|        - | 1979 | `		}` |
|       21 | 1980 | `		return TRUE;` |
|        - | 1981 | `	}` |
|       34 | 1982 | `	return FALSE;` |
|      119 | 1983 | `}` |
|        - | 1984 | `/*` |
|        - | 1985 | ` * Compare two ph7_values.` |
|        - | 1986 | ` * Return 0 if the values are equals, > 0 if pObj1 is greater than pObj2` |
|        - | 1987 | ` * or < 0 if pObj2 is greater than pObj1.` |
|        - | 1988 | ` * Type comparison table taken from the PHP language reference manual.` |
|        - | 1989 | ` * Comparisons of $x with PHP functions Expression` |
|        - | 1990 | ` *              gettype() 	empty() 	is_null() 	isset() 	boolean : if($x)` |
|        - | 1991 | ` * $x = ""; 	string 	    TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1992 | ` * $x = null 	NULL 	    TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1993 | ` * var $x; 	    NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1994 | ` * $x is undefined 	NULL 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 1995 | ` *  $x = array(); 	array 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1996 | ` * $x = false; 	boolean 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 1997 | ` * $x = true; 	boolean 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1998 | ` * $x = 1; 	    integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 1999 | ` * $x = 42; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2000 | ` * $x = 0; 	    integer 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 2001 | ` * $x = -1; 	integer 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2002 | ` * $x = "1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2003 | ` * $x = "0"; 	string 	TRUE 	FALSE 	TRUE 	FALSE` |
|        - | 2004 | ` * $x = "-1"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2005 | ` * $x = "php"; 	string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2006 | ` * $x = "true"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2007 | ` * $x = "false"; string 	FALSE 	FALSE 	TRUE 	TRUE` |
|        - | 2008 | ` *      Loose comparisons with ==` |
|        - | 2009 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 2010 | ` * TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 2011 | ` * FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 2012 | ` * 1 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2013 | ` * 0 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	TRUE 	TRUE` |
|        - | 2014 | ` * -1 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2015 | ` * "1" 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2016 | ` * "0" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2017 | ` * "-1" 	TRUE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2018 | ` * NULL 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	TRUE` |
|        - | 2019 | ` * array() 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	TRUE 	FALSE 	FALSE` |
|        - | 2020 | ` * "php" 	TRUE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 2021 | ` * "" 	FALSE 	TRUE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	TRUE` |
|        - | 2022 | ` *    Strict comparisons with ===` |
|        - | 2023 | ` * TRUE 	FALSE 	1 	0 	-1 	"1" 	"0" 	"-1" 	NULL 	array() 	"php" 	""` |
|        - | 2024 | ` * TRUE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2025 | ` * FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2026 | ` * 1 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2027 | ` * 0 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2028 | ` * -1 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2029 | ` * "1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2030 | ` * "0" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2031 | ` * "-1" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE 	FALSE` |
|        - | 2032 | ` * NULL 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE 	FALSE` |
|        - | 2033 | ` * array() 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE 	FALSE` |
|        - | 2034 | ` * "php" 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE 	FALSE` |
|        - | 2035 | ` * "" 	    FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	FALSE 	TRUE` |
|        - | 2036 | ` */` |
|  3900070 | 2037 | `PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest)` |
|        5 | 2038 | `{` |
|        - | 2039 | `	sxi32 iComb;` |
|        - | 2040 | `	sxi32 rc;` |
|  3900075 | 2041 | `	if( bStrict ){` |
|        - | 2042 | `		sxi32 iF1,iF2;` |
|        - | 2043 | `		/* Strict comparisons with === */` |
|  2184593 | 2044 | `		iF1 = pObj1->iFlags&~MEMOBJ_AUX;` |
|  2184593 | 2045 | `		iF2 = pObj2->iFlags&~MEMOBJ_AUX;` |
|        - | 2046 | `		/* MEMOBJ_INT beside MEMOBJ_REAL is not a TYPE: it is the speculative` |
|        - | 2047 | `		 * integer MemObjTryIntger leaves on a float whose value happens to be a` |
|        - | 2048 | `		 * whole number, and whether a given float carries one depends on which` |
|        - | 2049 | ``		 * road it took here. A literal `1.0` is built through PH7_MemObjToReal`` |
|        - | 2050 | ``		 * and arrives REAL\|INT; `(float)"1.0"` goes through OP_CVT_REAL, whose`` |
|        - | 2051 | `		 * MemObjSetType wipes the speculation, and arrives REAL. Comparing the` |
|        - | 2052 | ``		 * raw flags then made `(float)"1.0" === 1.0` FALSE while`` |
|        - | 2053 | ``		 * `(float)"2.5" === 2.5` was true -- the same wrong answer through`` |
|        - | 2054 | ``		 * in_array($x,[...],true), array_search(), an array `===` and every`` |
|        - | 2055 | `		 * other door that asks this comparator for identity. php has one float` |
|        - | 2056 | `		 * type, so drop the speculation before the type test; the numeric branch` |
|        - | 2057 | `		 * below already compares as reals whenever either side is one. */` |
|  2184593 | 2058 | `		if( iF1 & MEMOBJ_REAL ){ iF1 &= ~MEMOBJ_INT; }` |
|  2184593 | 2059 | `		if( iF2 & MEMOBJ_REAL ){ iF2 &= ~MEMOBJ_INT; }` |
|  2184593 | 2060 | `		if( iF1 != iF2 ){` |
|        - | 2061 | `			/* Not of the same type */` |
|   491194 | 2062 | `			return 1;` |
|        - | 2063 | `		}` |
|   847902 | 2064 | `	}` |
|        - | 2065 | `	/* Combine flag together */` |
|  3408886 | 2066 | `	iComb = pObj1->iFlags\|pObj2->iFlags;` |
|  3408881 | 2067 | `	if( !bStrict` |
|  2563384 | 2068 | `	 && (iComb & MEMOBJ_NULL) != 0` |
|   859354 | 2069 | `	 && (iComb & MEMOBJ_STRING) != 0` |
|       88 | 2070 | `	 && (iComb & (MEMOBJ_BOOL\|MEMOBJ_RES\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|        - | 2071 | `		/*` |
|        - | 2072 | `		 * PHP 8 comparison table: null loosely compared with a STRING is` |
|        - | 2073 | `		 * compared as the empty string (a string comparison), not through` |
|        - | 2074 | `		 * bool coercion — so null == "0" is FALSE and null < "0" is TRUE` |
|        - | 2075 | `		 * (php 7 and the historical PH7 behavior coerced both to bool,` |
|        - | 2076 | `		 * making any non-empty non-"0"-insensitive string "equal" to null).` |
|        - | 2077 | `		 * Convert the null side to "" and let the string branch below run.` |
|        - | 2078 | `		 */` |
|       45 | 2079 | `		if( pObj1->iFlags & MEMOBJ_NULL ){` |
|       31 | 2080 | `			PH7_MemObjToString(pObj1);` |
|       16 | 2081 | `		}else{` |
|       15 | 2082 | `			PH7_MemObjToString(pObj2);` |
|        - | 2083 | `		}` |
|       45 | 2084 | `		iComb = pObj1->iFlags\|pObj2->iFlags;` |
|       22 | 2085 | `	}` |
|  3408886 | 2086 | `	if( (pObj1->iFlags & MEMOBJ_RES) && (pObj2->iFlags & MEMOBJ_RES) ){` |
|        - | 2087 | `		/* php compares two resources by their ID. The boolean path below would` |
|        - | 2088 | `		 * call every live resource equal to every other, since all are truthy. */` |
|       62 | 2089 | `		sxu32 nId1 = PH7_VmResourceId(pObj1->pVm,pObj1->x.pOther);` |
|       62 | 2090 | `		sxu32 nId2 = PH7_VmResourceId(pObj2->pVm,pObj2->x.pOther);` |
|       62 | 2091 | `		return nId1 == nId2 ? 0 : (nId1 < nId2 ? -1 : 1);` |
|        - | 2092 | `	}` |
|  3408828 | 2093 | `	if( !bStrict && ((pObj1->iFlags ^ pObj2->iFlags) & MEMOBJ_OBJ) != 0 ){` |
|        - | 2094 | `		/*` |
|        - | 2095 | `		 * An object loosely compared with a NON-object: php's zend_compare has ONE` |
|        - | 2096 | `		 * rule for this, and it is not type precedence — it casts the OBJECT to the` |
|        - | 2097 | `		 * OTHER operand's type and compares the result, answering "the object is` |
|        - | 2098 | `		 * greater" only when that cast FAILS. PHL fell through to its own branches` |
|        - | 2099 | `		 * instead, and every one of them was wrong somewhere: a Stringable object` |
|        - | 2100 | ``		 * never compared as its string (`$s == "abc"` was FALSE, and`` |
|        - | 2101 | `		 * sort()/in_array()/array_search()/switch inherited that), an object against` |
|        - | 2102 | ``		 * an int compared as two bools (`$n < 20` was FALSE where php compares 1`` |
|        - | 2103 | `		 * with 20), an ARRAY was called greater than an object, and an object` |
|        - | 2104 | `		 * equalled every open resource.` |
|        - | 2105 | `		 *` |
|        - | 2106 | ``		 * `===` never arrives here: the flags differ, so the strict block above has`` |
|        - | 2107 | `		 * already answered 1.` |
|        - | 2108 | `		 */` |
|      328 | 2109 | `		int bObj1 = (pObj1->iFlags & MEMOBJ_OBJ) != 0;` |
|      328 | 2110 | `		ph7_value *pSelf  = bObj1 ? pObj1 : pObj2;` |
|      328 | 2111 | `		ph7_value *pOther = bObj1 ? pObj2 : pObj1;` |
|        - | 2112 | `		ph7_value sCast;` |
|        - | 2113 | `		{` |
|        - | 2114 | `			/* ...unless the object's class declares php's compare handler, which is` |
|        - | 2115 | ``			 * asked about a scalar partner too: `Number('1.5') == '1.50'` is TRUE`` |
|        - | 2116 | `			 * where the cast rule would compare two strings. A handler that does not` |
|        - | 2117 | `			 * recognize the value falls through to the cast below. */` |
|      328 | 2118 | `			sxi32 iNative = 1;` |
|      490 | 2119 | `			if( PH7_ClassNativeCmpValue((ph7_class_instance *)pSelf->x.pOther,pOther,` |
|      162 | 2120 | `				!bObj1,&iNative) ){` |
|        - | 2121 | `				/* The hook was told which side it is on and has already flipped its` |
|        - | 2122 | `				 * ordering; the uncomparable 1 is deliberately NOT flipped, which is` |
|        - | 2123 | `				 * what leaves every relational spelling false from both directions. */` |
|       96 | 2124 | `				return iNative;` |
|        - | 2125 | `			}` |
|        - | 2126 | `		}` |
|      234 | 2127 | `		if( MemObjCmpCastObject(pSelf,pOther,&sCast) ){` |
|        - | 2128 | `			/* sCast is a scalar, so the recursion cannot come back through here. */` |
|       97 | 2129 | `			rc = bObj1 ? PH7_MemObjCmp(&sCast,pOther,bStrict,iNest)` |
|       56 | 2130 | `			           : PH7_MemObjCmp(pOther,&sCast,bStrict,iNest);` |
|      103 | 2131 | `			PH7_MemObjRelease(&sCast);` |
|      103 | 2132 | `			return rc;` |
|        - | 2133 | `		}` |
|      134 | 2134 | `		PH7_MemObjRelease(&sCast);` |
|        - | 2135 | `		/* Cast refused (null, array, resource, or no __toString): object is greater. */` |
|      134 | 2136 | `		return bObj1 ? 1 : -1;` |
|        - | 2137 | `	}` |
|  3408504 | 2138 | `	if( iComb & (MEMOBJ_NULL\|MEMOBJ_BOOL) ){` |
|        - | 2139 | `		/* Convert to boolean: Keep in mind FALSE < TRUE. php decides null and bool` |
|        - | 2140 | `		 * this way and nothing else -- a RESOURCE used to be decided here too,` |
|        - | 2141 | `		 * which made every open one equal to every other truthy value. */` |
|    70524 | 2142 | `		if( (pObj1->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    40846 | 2143 | `			MemObjToBoolQuiet(pObj1);   /* php's comparison says nothing about a NaN */` |
|    20408 | 2144 | `		}` |
|    70524 | 2145 | `		if( (pObj2->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    39097 | 2146 | `			MemObjToBoolQuiet(pObj2);` |
|    19534 | 2147 | `		}` |
|    70524 | 2148 | `		return (sxi32)((pObj1->x.iVal != 0) - (pObj2->x.iVal != 0));` |
|  3337985 | 2149 | `	}else if ( iComb & MEMOBJ_HASHMAP ){` |
|        - | 2150 | `		/* Hashmap aka 'array' comparison */` |
|      488 | 2151 | `		if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 2152 | `			/* Array is always greater */` |
|       37 | 2153 | `			return -1;` |
|        - | 2154 | `		}` |
|      452 | 2155 | `		if( (pObj2->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 2156 | `			/* Array is always greater */` |
|       21 | 2157 | `			return 1;` |
|        - | 2158 | `		}` |
|        - | 2159 | `		/* Perform the comparison */` |
|      432 | 2160 | `		rc = PH7_HashmapCmp((ph7_hashmap *)pObj1->x.pOther,(ph7_hashmap *)pObj2->x.pOther,bStrict);` |
|      432 | 2161 | `		return rc;` |
|  3337502 | 2162 | `	}else if(iComb & MEMOBJ_OBJ ){` |
|        - | 2163 | `		/* Object comparison. Only a pair of objects can get here: a strict compare` |
|        - | 2164 | `		 * of mixed types answered 1 at the top, and a loose one went through the` |
|        - | 2165 | `		 * cast rule above — but keep the guards, so no future flag combination can` |
|        - | 2166 | `		 * hand PH7_ClassInstanceCmp something that is not an instance. */` |
|      961 | 2167 | `		if( (pObj1->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2168 | `			/* Object is always greater */` |
|      ! 0 | 2169 | `			return -1;` |
|        - | 2170 | `		}` |
|      961 | 2171 | `		if( (pObj2->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - | 2172 | `			/* Object is always greater */` |
|      ! 0 | 2173 | `			return 1;` |
|        - | 2174 | `		}` |
|        - | 2175 | `		/* Perform the comparison */` |
|      961 | 2176 | `		rc = PH7_ClassInstanceCmp((ph7_class_instance *)pObj1->x.pOther,(ph7_class_instance *)pObj2->x.pOther,bStrict,iNest);` |
|      961 | 2177 | `		return rc;` |
|  3336546 | 2178 | `	}else if( !VmIsUnorderedCmp(pObj1,pObj2) && (iComb & MEMOBJ_RES) ){` |
|        - | 2179 | `		/* php compares a resource with a NON-resource as its ID -- the number` |
|        - | 2180 | ``		 * `(int)$fp` answers -- and the other side takes php's LEGACY`` |
|        - | 2181 | `		 * scalar-to-number conversion, not php 8's saner string rule, because` |
|        - | 2182 | ``		 * this comparison never reaches that rule: `$fp == "5abc"` is TRUE for`` |
|        - | 2183 | ``		 * resource #5 and `$fp > "x"` compares 5 with 0. PHL compared the pair as`` |
|        - | 2184 | `		 * BOOLEANS, so an open resource equalled every non-empty string, every` |
|        - | 2185 | `		 * non-zero number and every other open resource, was GREATER than the` |
|        - | 2186 | ``		 * empty array, and `max($fp, 10)` answered the resource.`` |
|        - | 2187 | `		 *` |
|        - | 2188 | `		 * The two-resource case is decided above (by ID), null and bool before` |
|        - | 2189 | `		 * that (php's bool comparison), an array above this (an array is` |
|        - | 2190 | `		 * greater), an object by the cast rule, and a NaN by the unordered one --` |
|        - | 2191 | `		 * exactly php's order. */` |
|      229 | 2192 | `		int bRes1 = (pObj1->iFlags & MEMOBJ_RES) != 0;` |
|      229 | 2193 | `		ph7_value *pRes = bRes1 ? pObj1 : pObj2;` |
|      229 | 2194 | `		ph7_value *pOther = bRes1 ? pObj2 : pObj1;` |
|      229 | 2195 | `		sxi64 iId = (sxi64)PH7_VmResourceId(pRes->pVm,pRes->x.pOther);` |
|      229 | 2196 | `		PH7_MemObjToNumeric(pOther);` |
|      229 | 2197 | `		if( pOther->iFlags & MEMOBJ_REAL ){` |
|       57 | 2198 | `			ph7_real rId = (ph7_real)iId;` |
|       57 | 2199 | `			rc = rId > pOther->rVal ? 1 : (rId < pOther->rVal ? -1 : 0);` |
|       29 | 2200 | `		}else{` |
|      173 | 2201 | `			rc = iId > pOther->x.iVal ? 1 : (iId < pOther->x.iVal ? -1 : 0);` |
|        - | 2202 | `		}` |
|      229 | 2203 | `		return bRes1 ? rc : -rc;` |
|  3336318 | 2204 | `	}else if( VmIsUnorderedCmp(pObj1,pObj2) ){` |
|        - | 2205 | `		/* A NaN against a number or a string: php answers 1 in BOTH directions` |
|        - | 2206 | ``		 * (`NAN <=> 1` and `1 <=> NAN` are both 1), which is what leaves every`` |
|        - | 2207 | `		 * relational operator false at once. The rule lives HERE, not only in the` |
|        - | 2208 | `		 * operator arms, because everything else that orders values goes through` |
|        - | 2209 | ``		 * this comparator with no arm of its own: `in_array(NAN, ["NAN"])` was TRUE`` |
|        - | 2210 | ``		 * (php: false), `array_search` found it, and a `switch` matched it -- all`` |
|        - | 2211 | `		 * because the string branch below rendered the NaN as the bytes "NAN" and` |
|        - | 2212 | `		 * compared those. The precedence php gives null, bool, array and object is` |
|        - | 2213 | `		 * already spent above: VmIsUnorderedCmp screens those flags out, so` |
|        - | 2214 | ``		 * `NAN == true` stays the bool comparison it is there. */`` |
|      240 | 2215 | `		return 1;` |
|  3336080 | 2216 | `	}else if ( iComb & MEMOBJ_STRING ){` |
|        - | 2217 | `		SyString s1,s2;` |
|  1503709 | 2218 | `		if( !bStrict ){` |
|        - | 2219 | `			/*` |
|        - | 2220 | `			 * PHP 8 "saner string to number comparisons" (RFC): a numeric` |
|        - | 2221 | `			 * comparison is performed only when BOTH operands are numbers or` |
|        - | 2222 | `			 * numeric strings. A number compared with a NON-numeric string is` |
|        - | 2223 | `			 * compared as strings, with the number cast to its string form —` |
|        - | 2224 | `			 * so 0 == "abc" is false, "abc" < 10 is false, and max("abc",10)` |
|        - | 2225 | `			 * is "abc". (PHP 7 cast the non-numeric string to 0 and compared` |
|        - | 2226 | `			 * numerically; comparing when EITHER side was numeric is what this` |
|        - | 2227 | `			 * replaces.) Two non-numeric strings, or one numeric and one` |
|        - | 2228 | `			 * non-numeric string, still fall through to the string comparison` |
|        - | 2229 | `			 * below, unchanged.` |
|        - | 2230 | `			 */` |
|   308966 | 2231 | `			if( PH7_MemObjIsNumeric(pObj1) && PH7_MemObjIsNumeric(pObj2) ){` |
|        - | 2232 | `				/*` |
|        - | 2233 | `				 * Two INTEGER-shaped numeric STRINGS past the int64 range are not` |
|        - | 2234 | `				 * compared through their doubles, because the conversion threw away` |
|        - | 2235 | `				 * the digits that tell them apart. php has two rules for them, both` |
|        - | 2236 | `				 * only for a string against a string (a string against an int VALUE` |
|        - | 2237 | `				 * really does compare as doubles, so` |
|        - | 2238 | ``				 * `"9223372036854775808" == PHP_INT_MAX` is true):`` |
|        - | 2239 | `				 *` |
|        - | 2240 | `				 *  - Same side, same double: compare the BYTES. So` |
|        - | 2241 | `				 *    "9223372036854775808" == "9223372036854775809" is FALSE, and it` |
|        - | 2242 | `				 *    is the RAW bytes -- sign, leading zeros and whitespace included` |
|        - | 2243 | `				 *    -- so "9223372036854775808" != "09223372036854775808" too. Two` |
|        - | 2244 | `				 *    digit runs that both overflow to infinity land here as well.` |
|        - | 2245 | `				 *  - One side past the range, the other an integer-shaped string that` |
|        - | 2246 | `				 *    FITS: the overflowing side simply IS the greater (or lesser)` |
|        - | 2247 | `				 *    one, no conversion involved -- which is why` |
|        - | 2248 | `				 *    "9223372036854775808" > "9223372036854775807" even though both` |
|        - | 2249 | `				 *    reach the same double.` |
|        - | 2250 | `				 *` |
|        - | 2251 | `				 * Everything else stays numeric: opposite sides, unequal doubles, a` |
|        - | 2252 | `				 * float-SHAPED operand, or anything that is not a string.` |
|        - | 2253 | `				 */` |
|      431 | 2254 | `				int bBytes = 0;` |
|        - | 2255 | `				{` |
|      431 | 2256 | `					ph7_real r1 = 0, r2 = 0;` |
|      431 | 2257 | `					int iOf1 = 0, iOf2 = 0;` |
|      431 | 2258 | `					int bInt1 = MemObjStringIntShape(pObj1,&iOf1,&r1);` |
|      431 | 2259 | `					int bInt2 = MemObjStringIntShape(pObj2,&iOf2,&r2);` |
|      431 | 2260 | `					if( iOf1 != 0 && iOf1 == iOf2 && r1 == r2 ){` |
|      101 | 2261 | `						bBytes = 1;` |
|      381 | 2262 | `					}else if( iOf1 != 0 && bInt2 && iOf2 == 0 ){` |
|       36 | 2263 | `						return iOf1;` |
|      305 | 2264 | `					}else if( iOf2 != 0 && bInt1 && iOf1 == 0 ){` |
|       19 | 2265 | `						return -iOf2;` |
|        - | 2266 | `					}` |
|        - | 2267 | `				}` |
|      387 | 2268 | `				if( !bBytes ){` |
|        - | 2269 | `					/* Perform a numeric comparison */` |
|      287 | 2270 | `					goto Numeric;` |
|        - | 2271 | `				}` |
|       50 | 2272 | `			}` |
|   154309 | 2273 | `		}` |
|        - | 2274 | `		/* Perform a strict string comparison.*/` |
|  1503381 | 2275 | `		if( (pObj1->iFlags&MEMOBJ_STRING) == 0 ){` |
|       35 | 2276 | `			PH7_MemObjToString(pObj1);` |
|       17 | 2277 | `		}` |
|  1503381 | 2278 | `		if( (pObj2->iFlags&MEMOBJ_STRING) == 0 ){` |
|       47 | 2279 | `			PH7_MemObjToString(pObj2);` |
|       23 | 2280 | `		}` |
|  1503381 | 2281 | `		SyStringInitFromBuf(&s1,SyBlobData(&pObj1->sBlob),SyBlobLength(&pObj1->sBlob));` |
|  1503381 | 2282 | `		SyStringInitFromBuf(&s2,SyBlobData(&pObj2->sBlob),SyBlobLength(&pObj2->sBlob));` |
|        - | 2283 | `		/*` |
|        - | 2284 | `		 * Strings are compared using memcmp(). If one value is an exact prefix of the` |
|        - | 2285 | `		 * other, then the shorter value is less than the longer value.` |
|        - | 2286 | `		 */` |
|  1503381 | 2287 | `		rc = SyMemcmp((const void *)s1.zString,(const void *)s2.zString,SXMIN(s1.nByte,s2.nByte));` |
|  1503381 | 2288 | `		if( rc == 0 ){` |
|   477052 | 2289 | `			if( s1.nByte != s2.nByte ){` |
|    33298 | 2290 | `				rc = s1.nByte < s2.nByte ? -1 : 1;` |
|    16647 | 2291 | `			}` |
|   238504 | 2292 | `		}` |
|  1503381 | 2293 | `		return rc;` |
|  1832376 | 2294 | `	}else if( iComb & (MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|   914633 | 2295 | `Numeric:` |
|        - | 2296 | `		/* Perform a numeric comparison if one of the operand is numeric(integer or real) */` |
|  1832660 | 2297 | `		if( (pObj1->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      227 | 2298 | `			PH7_MemObjToNumeric(pObj1);` |
|      110 | 2299 | `		}` |
|  1832660 | 2300 | `		if( (pObj2->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL)) == 0 ){` |
|      245 | 2301 | `			PH7_MemObjToNumeric(pObj2);` |
|      119 | 2302 | `		}` |
|  1832660 | 2303 | `		if( (pObj1->iFlags & pObj2->iFlags & MEMOBJ_INT) == 0) {` |
|        - | 2304 | `			/*` |
|        - | 2305 | `			 * Symisc eXtension to the PHP language:` |
|        - | 2306 | `			 *  Floating point comparison is introduced and works as expected.` |
|        - | 2307 | `			 */` |
|        - | 2308 | `			ph7_real r1,r2;` |
|        - | 2309 | `			/* Compare as reals */` |
|      874 | 2310 | `			if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       45 | 2311 | `				PH7_MemObjToReal(pObj1);` |
|       22 | 2312 | `			}` |
|      874 | 2313 | `			r1 = pObj1->rVal;` |
|      874 | 2314 | `			if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       27 | 2315 | `				PH7_MemObjToReal(pObj2);` |
|       13 | 2316 | `			}` |
|      874 | 2317 | `			r2 = pObj2->rVal;` |
|      874 | 2318 | `			if( PH7_IS_NAN(r1) \|\| PH7_IS_NAN(r2) ){` |
|        - | 2319 | `				/*` |
|        - | 2320 | `				 * php's answer for an unordered pair, from either side: 1. The` |
|        - | 2321 | `				 * branch above catches every NaN that arrives AS a float; this one` |
|        - | 2322 | `				 * is for a NaN that only appears once both operands have been` |
|        - | 2323 | `				 * converted, and it must agree with it -- an antisymmetric answer` |
|        - | 2324 | ``				 * here (the old `NaN equals NaN, and is greater than everything`` |
|        - | 2325 | ``				 * else`) is what made `NAN === NAN` true and `1.5 > NAN` disagree`` |
|        - | 2326 | ``				 * with `NAN < 1.5`.`` |
|        - | 2327 | `				 */` |
|      ! 0 | 2328 | `				return 1;` |
|        - | 2329 | `			}` |
|      874 | 2330 | `			if( r1 > r2 ){` |
|       49 | 2331 | `				return 1;` |
|      828 | 2332 | `			}else if( r1 < r2 ){` |
|      228 | 2333 | `				return -1;` |
|        - | 2334 | `			}` |
|      603 | 2335 | `			return 0;` |
|      ! 0 | 2336 | `		}else{` |
|        - | 2337 | `			/* Integer comparison */` |
|  1831790 | 2338 | `			if( pObj1->x.iVal > pObj2->x.iVal ){` |
|   323258 | 2339 | `				return 1;` |
|  1508537 | 2340 | `			}else if( pObj1->x.iVal < pObj2->x.iVal ){` |
|  1086006 | 2341 | `				return -1;` |
|        - | 2342 | `			}` |
|   422536 | 2343 | `			return 0;` |
|        - | 2344 | `		}` |
|        - | 2345 | `	}` |
|        - | 2346 | `	/* NOT REACHED */` |
|      ! 0 | 2347 | `	return 0;` |
|  1952406 | 2348 | `}` |
|        - | 2349 | `/*` |
|        - | 2350 | ` * Perform an addition operation of two ph7_values.` |
|        - | 2351 | ` * The reason this function is implemented here rather than 'vm.c'` |
|        - | 2352 | ` * is that the '+' operator is overloaded.` |
|        - | 2353 | ` * That is,the '+' operator is used for arithmetic operation and also` |
|        - | 2354 | ` * used for operation on arrays [i.e: union]. When used with an array` |
|        - | 2355 | ` * The + operator returns the right-hand array appended to the left-hand array.` |
|        - | 2356 | ` * For keys that exist in both arrays, the elements from the left-hand array` |
|        - | 2357 | ` * will be used, and the matching elements from the right-hand array will` |
|        - | 2358 | ` * be ignored.` |
|        - | 2359 | ` * This function take care of handling all the scenarios.` |
|        - | 2360 | ` */` |
|   511261 | 2361 | `PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore)` |
|        5 | 2362 | `{` |
|   511266 | 2363 | `	if( ((pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 2364 | `			/* Arithemtic operation */` |
|   505289 | 2365 | `			PH7_MemObjToNumeric(pObj1);` |
|   505289 | 2366 | `			PH7_MemObjToNumeric(pObj2);` |
|   505289 | 2367 | `			if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_REAL ){` |
|        - | 2368 | `				/* Floating point arithmetic */` |
|        - | 2369 | `				ph7_real a,b;` |
|      141 | 2370 | `				if( (pObj1->iFlags & MEMOBJ_REAL) == 0 ){` |
|       46 | 2371 | `					PH7_MemObjToReal(pObj1);` |
|       22 | 2372 | `				}` |
|      141 | 2373 | `				if( (pObj2->iFlags & MEMOBJ_REAL) == 0 ){` |
|       57 | 2374 | `					PH7_MemObjToReal(pObj2);` |
|       28 | 2375 | `				}` |
|      141 | 2376 | `				a = pObj1->rVal;` |
|      141 | 2377 | `				b = pObj2->rVal;` |
|      141 | 2378 | `				pObj1->rVal = a+b;` |
|      141 | 2379 | `				MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 2380 | `				/* Try to get an integer representation also */` |
|      141 | 2381 | `				MemObjTryIntger(&(*pObj1));` |
|       72 | 2382 | `			}else{` |
|        - | 2383 | `				/* Integer arithmetic; PHP promotes an overflowing sum to float.` |
|        - | 2384 | `				 * The integer-only build (PH7_OMIT_FLOATING_POINT) has no float` |
|        - | 2385 | `				 * type, so it wraps like OP_POW's OMIT path. */` |
|        - | 2386 | `				sxi64 a,b,r;` |
|   505151 | 2387 | `				a = pObj1->x.iVal;` |
|   505151 | 2388 | `				b = pObj2->x.iVal;` |
|   505151 | 2389 | `				if( PH7_ADD_OVERFLOW64(a,b,&r) ){` |
|        - | 2390 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       63 | 2391 | `					pObj1->rVal = (ph7_real)a + (ph7_real)b;` |
|       63 | 2392 | `					MemObjSetType(pObj1,MEMOBJ_REAL);` |
|        - | 2393 | `#else` |
|        - | 2394 | `					pObj1->x.iVal = r;` |
|        - | 2395 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 2396 | `#endif` |
|       32 | 2397 | `				}else{` |
|   505089 | 2398 | `					pObj1->x.iVal = r;` |
|   505089 | 2399 | `					MemObjSetType(pObj1,MEMOBJ_INT);` |
|        - | 2400 | `				}` |
|        - | 2401 | `			}` |
|   252728 | 2402 | `	}else{` |
|     5982 | 2403 | `		if( (pObj1->iFlags\|pObj2->iFlags) & MEMOBJ_HASHMAP ){` |
|        - | 2404 | `			ph7_hashmap *pMap;` |
|        - | 2405 | `			sxi32 rc;` |
|     5982 | 2406 | `			if( bAddStore ){` |
|        - | 2407 | `				/* Do not duplicate the hashmap,use the left one since its an add&store operation.` |
|        - | 2408 | `				 */` |
|        5 | 2409 | `				if( (pObj1->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - | 2410 | `					/* Force a hashmap cast */` |
|      ! 0 | 2411 | `					rc = PH7_MemObjToHashmap(pObj1);` |
|      ! 0 | 2412 | `					if( rc != SXRET_OK ){` |
|      ! 0 | 2413 | `						PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 2414 | `						return rc;` |
|        - | 2415 | `					}` |
|      ! 0 | 2416 | `				}` |
|        - | 2417 | `				/* COW separate before in-place mutation */` |
|        5 | 2418 | `				pMap = PH7_HashmapCowSeparate(pObj1->pVm,pObj1);` |
|        3 | 2419 | `			}else{` |
|        - | 2420 | `				/* Create a new hashmap */` |
|     5978 | 2421 | `				pMap = PH7_NewHashmap(pObj1->pVm,0,0);` |
|     5978 | 2422 | `				if( pMap == 0){` |
|      ! 0 | 2423 | `					PH7_VmThrowError(pObj1->pVm,0,PH7_CTX_ERR,"PH7 is running out of memory while creating array");` |
|      ! 0 | 2424 | `					return SXERR_MEM;` |
|        - | 2425 | `				}` |
|        - | 2426 | `			}` |
|     5982 | 2427 | `			if( !bAddStore ){` |
|     5978 | 2428 | `				if(pObj1->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 2429 | `					/* Perform a hashmap duplication */` |
|     5978 | 2430 | `					PH7_HashmapDup((ph7_hashmap *)pObj1->x.pOther,pMap);` |
|     2987 | 2431 | `				}else{` |
|      ! 0 | 2432 | `					if((pObj1->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 2433 | `						/* Simple insertion */` |
|      ! 0 | 2434 | `						PH7_HashmapInsert(pMap,0,pObj1);` |
|      ! 0 | 2435 | `					}` |
|        - | 2436 | `				}` |
|     2982 | 2437 | `			}` |
|        - | 2438 | `			/* Perform the union */` |
|     5982 | 2439 | `			if(pObj2->iFlags & MEMOBJ_HASHMAP ){` |
|     5982 | 2440 | `				PH7_HashmapUnion(pMap,(ph7_hashmap *)pObj2->x.pOther);` |
|     2989 | 2441 | `			}else{` |
|      ! 0 | 2442 | `				if((pObj2->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 2443 | `					/* Simple insertion */` |
|      ! 0 | 2444 | `					PH7_HashmapInsert(pMap,0,pObj2);` |
|      ! 0 | 2445 | `				}` |
|        - | 2446 | `			}` |
|        - | 2447 | `			/* Reflect the change */` |
|     5982 | 2448 | `			if( pObj1->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 2449 | `				SyBlobRelease(&pObj1->sBlob);` |
|      ! 0 | 2450 | `			}` |
|     5982 | 2451 | `			pObj1->x.pOther = pMap;` |
|     5982 | 2452 | `			MemObjSetType(pObj1,MEMOBJ_HASHMAP);` |
|     2984 | 2453 | `		}` |
|        - | 2454 | `	}` |
|   511266 | 2455 | `	return SXRET_OK;` |
|   255712 | 2456 | `}` |
|        - | 2457 | `/*` |
|        - | 2458 | ` * Return a printable representation of the type of a given` |
|        - | 2459 | ` * ph7_value.` |
|        - | 2460 | ` */` |
|       10 | 2461 | `PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal)` |
|        3 | 2462 | `{` |
|       13 | 2463 | `	const char *zType = "";` |
|       13 | 2464 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 | 2465 | `		zType = "null";` |
|       13 | 2466 | `	}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|        - | 2467 | `		/* REAL is authoritative over a cached MEMOBJ_INT: an integer-valued` |
|        - | 2468 | `		 * real (e.g. 1.0) is reported as "double", matching PHP's gettype(). */` |
|      ! 0 | 2469 | `		zType = "double";` |
|       13 | 2470 | `	}else if( pVal->iFlags & MEMOBJ_INT ){` |
|      ! 0 | 2471 | `		zType = "int";` |
|       13 | 2472 | `	}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|        8 | 2473 | `		zType = "string";` |
|        9 | 2474 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 | 2475 | `		zType = "bool";` |
|        6 | 2476 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|        6 | 2477 | `		zType = "array";` |
|        2 | 2478 | `	}else if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 2479 | `		zType = "object";` |
|      ! 0 | 2480 | `	}else if( pVal->iFlags & MEMOBJ_RES ){` |
|      ! 0 | 2481 | `		zType = "resource";` |
|      ! 0 | 2482 | `	}` |
|       13 | 2483 | `	return zType;` |
|        3 | 2484 | `}` |
|        - | 2485 | `/*` |
|        - | 2486 | ` * Dump a ph7_value [i.e: get a printable representation of it's type and contents.].` |
|        - | 2487 | ` * Store the dump in the given blob.` |
|        - | 2488 | ` */` |
|        - | 2489 | `/*` |
|        - | 2490 | ` * php's var_dump float shape (serialize_precision = -1): the SHORTEST decimal` |
|        - | 2491 | ` * string that round-trips to the same double — 0.1+0.2 dumps every digit` |
|        - | 2492 | ` * (0.30000000000000004), 1.0 dumps "1" — pushed through the same` |
|        - | 2493 | ` * exponent/fraction normalization as echo (PH7_PhpFloatShape: uppercase E,` |
|        - | 2494 | ` * "1.0E+100"). Distinct from echo/casts, which use EG(precision)=14.` |
|        - | 2495 | ` */` |
|      254 | 2496 | `static void MemObjDumpRealValue(SyBlob *pOut,ph7_real rVal)` |
|        5 | 2497 | `{` |
|        - | 2498 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - | 2499 | `	/* var_dump renders floats at serialize_precision = -1 — the SHORTEST decimal` |
|        - | 2500 | `	 * that round-trips, formatted by php's gcvt(ndigit=17) fixed-vs-exponential` |
|        - | 2501 | `	 * rule (exponential only when the leading-digit exponent e >= 17 or e <= -5,` |
|        - | 2502 | `	 * so 1500.0 -> "1500", 1e20 -> "1.0E+20"). That is exactly the shape serialize/` |
|        - | 2503 | `	 * var_export/json already emit, so share their helper. The old code searched` |
|        - | 2504 | `	 * "%.*G" from precision 1 upward, but %G's own exponential threshold moves with` |
|        - | 2505 | `	 * the precision, so a low-precision round-trip (1500.0 at %.2G) came back as` |
|        - | 2506 | `	 * "1.5E+3" — a rendering-only wrong answer this delegation removes. */` |
|      259 | 2507 | `	PH7_AppendShortestReal(pOut,rVal);` |
|        - | 2508 | `#else` |
|        - | 2509 | `	if( PH7_IS_NAN(rVal) ){` |
|        - | 2510 | `		SyBlobAppend(&(*pOut),"NAN",3);` |
|        - | 2511 | `	}else if( PH7_IS_INF(rVal) ){` |
|        - | 2512 | `		SyBlobAppend(&(*pOut),rVal < 0.0 ? "-INF" : "INF",rVal < 0.0 ? 4 : 3);` |
|        - | 2513 | `	}else{` |
|        - | 2514 | `		SyBlobFormat(&(*pOut),"%.15g",rVal);` |
|        - | 2515 | `	}` |
|        - | 2516 | `#endif` |
|      259 | 2517 | `}` |
|        - | 2518 | `/*` |
|        - | 2519 | ` * Emit a value's print_r INLINE representation (php: the echo conversion,` |
|        - | 2520 | ` * except true -> "1" and false/null -> ""). Containers never come through` |
|        - | 2521 | ` * here — the entry renderers recurse into the container dumpers instead.` |
|        - | 2522 | ` */` |
|     1174 | 2523 | `PH7_PRIVATE void PH7_MemObjPrintRInline(SyBlob *pOut,ph7_value *pObj)` |
|        5 | 2524 | `{` |
|        - | 2525 | `	/* print_r RENDERS through the string coercion -- unlike var_dump and` |
|        - | 2526 | `	 * var_export, which describe the value instead -- so php's NaN warning` |
|        - | 2527 | `	 * belongs here too, once per value it prints. */` |
|     1174 | 2528 | `	if( (pObj->iFlags & (MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_REAL` |
|      595 | 2529 | `	 && pObj->pVm && PH7_IS_NAN(pObj->rVal) ){` |
|        3 | 2530 | `		VmErrorFormat(pObj->pVm,PH7_CTX_WARNING,` |
|        - | 2531 | `			"unexpected NAN value was coerced to string");` |
|        1 | 2532 | `	}` |
|     1179 | 2533 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|      107 | 2534 | `		return;` |
|        - | 2535 | `	}` |
|     1073 | 2536 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|       70 | 2537 | `		if( pObj->x.iVal != 0 ){` |
|       21 | 2538 | `			SyBlobAppend(&(*pOut),"1",sizeof(char));` |
|       10 | 2539 | `		}` |
|       70 | 2540 | `		return;` |
|        - | 2541 | `	}` |
|     1005 | 2542 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|        - | 2543 | `		/* Strings already hold their bytes (MemObjStringValue only CONVERTS` |
|        - | 2544 | `		 * non-strings into the output) */` |
|      607 | 2545 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      591 | 2546 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|      293 | 2547 | `		}` |
|      607 | 2548 | `		return;` |
|        - | 2549 | `	}` |
|      403 | 2550 | `	MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      592 | 2551 | `}` |
|    18563 | 2552 | `PH7_PRIVATE sxi32 PH7_MemObjDump(` |
|        - | 2553 | `	SyBlob *pOut,      /* Store the dump here */` |
|        - | 2554 | `	ph7_value *pObj,   /* Dump this */` |
|        - | 2555 | `	int ShowType,      /* TRUE for var_dump; FALSE for print_r */` |
|        - | 2556 | `	int nTab,          /* Indent in SPACES: var_dump = this value's own line;` |
|        - | 2557 | `	                    * print_r = the container's parenthesis column */` |
|        - | 2558 | `	int nDepth,        /* Nesting level */` |
|        - | 2559 | `	int isRef          /* TRUE if referenced entry (var_dump prints '&') */` |
|        - | 2560 | `	)` |
|        5 | 2561 | `{` |
|    18568 | 2562 | `	sxi32 rc = SXRET_OK;` |
|        - | 2563 | `	int i;` |
|    18568 | 2564 | `	if( !ShowType ){` |
|        - | 2565 | `		/* ---- print_r ---- php prints scalars inline with NO newline; only` |
|        - | 2566 | `		 * containers render the Array/Object block (which the container` |
|        - | 2567 | `		 * dumpers terminate with ")\n"). References carry no marker. */` |
|      469 | 2568 | `		if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      323 | 2569 | `			return PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 2570 | `		}` |
|      151 | 2571 | `		if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      147 | 2572 | `			return PH7_ClassInstanceDump(&(*pOut),(ph7_class_instance *)pObj->x.pOther,FALSE,nTab,nDepth+1);` |
|        - | 2573 | `		}` |
|        5 | 2574 | `		PH7_MemObjPrintRInline(&(*pOut),pObj);` |
|        5 | 2575 | `		return SXRET_OK;` |
|        - | 2576 | `	}` |
|        - | 2577 | `	/* ---- var_dump ---- every value renders on its own line at nTab spaces,` |
|        - | 2578 | `	 * php's exact shapes: bool(true), NULL, int(n), float(shortest),` |
|        - | 2579 | `	 * string(N) "s", array(N) { … }, object(C)#id (n) { … }, &-references. */` |
|    30576 | 2580 | `	for( i = 0 ; i < nTab ; i++ ){` |
|    12477 | 2581 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     6241 | 2582 | `	}` |
|    18104 | 2583 | `	if( isRef ){` |
|      118 | 2584 | `		SyBlobAppend(&(*pOut),"&",sizeof(char));` |
|       58 | 2585 | `	}` |
|    18104 | 2586 | `	if( (pObj->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      319 | 2587 | `		ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|      319 | 2588 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|        - | 2589 | ``			/* php 8.1: var_dump of an enum case prints `enum(S::A)` — no body */`` |
|        7 | 2590 | `			ph7_value *pName = PH7_EnumCaseNameValue(pInst);` |
|        7 | 2591 | `			SyBlobFormat(&(*pOut),"enum(%z::",&pInst->pClass->sName);` |
|        7 | 2592 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|        7 | 2593 | `				SyBlobAppend(&(*pOut),SyBlobData(&pName->sBlob),SyBlobLength(&pName->sBlob));` |
|        3 | 2594 | `			}` |
|        7 | 2595 | `			SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        7 | 2596 | `			return SXRET_OK;` |
|        - | 2597 | `		}` |
|      313 | 2598 | `		rc = PH7_ClassInstanceDump(&(*pOut),pInst,TRUE,nTab,nDepth+1);` |
|      313 | 2599 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      313 | 2600 | `		return rc;` |
|        - | 2601 | `	}` |
|    17790 | 2602 | `	if( pObj->iFlags & MEMOBJ_NULL ){` |
|      736 | 2603 | `		SyBlobAppend(&(*pOut),"NULL\n",sizeof("NULL\n")-1);` |
|      736 | 2604 | `		return SXRET_OK;` |
|        - | 2605 | `	}` |
|    17059 | 2606 | `	if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|     1610 | 2607 | `		rc = PH7_HashmapDump(&(*pOut),(ph7_hashmap *)pObj->x.pOther,TRUE,nTab,nDepth+1);` |
|     1610 | 2608 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     1610 | 2609 | `		return rc;` |
|        - | 2610 | `	}` |
|    15454 | 2611 | `	if( pObj->iFlags & MEMOBJ_BOOL ){` |
|     5240 | 2612 | `		if( pObj->x.iVal != 0 ){` |
|     3013 | 2613 | `			SyBlobAppend(&(*pOut),"bool(true)\n",sizeof("bool(true)\n")-1);` |
|     1502 | 2614 | `		}else{` |
|     2232 | 2615 | `			SyBlobAppend(&(*pOut),"bool(false)\n",sizeof("bool(false)\n")-1);` |
|        - | 2616 | `		}` |
|     5240 | 2617 | `		return SXRET_OK;` |
|        - | 2618 | `	}` |
|    10219 | 2619 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|        - | 2620 | `		/* Checked BEFORE the int flag: an integer-valued real carries a cached` |
|        - | 2621 | `		 * MEMOBJ_INT view too, and php dumps it as float(1). */` |
|      259 | 2622 | `		SyBlobAppend(&(*pOut),"float(",sizeof("float(")-1);` |
|      259 | 2623 | `		MemObjDumpRealValue(&(*pOut),pObj->rVal);` |
|      259 | 2624 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|      259 | 2625 | `		return SXRET_OK;` |
|        - | 2626 | `	}` |
|     9965 | 2627 | `	if( pObj->iFlags & MEMOBJ_INT ){` |
|     4443 | 2628 | `		SyBlobFormat(&(*pOut),"int(%qd)",pObj->x.iVal);` |
|     4443 | 2629 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|     4443 | 2630 | `		return SXRET_OK;` |
|        - | 2631 | `	}` |
|     5527 | 2632 | `	if( pObj->iFlags & MEMOBJ_STRING ){` |
|     5527 | 2633 | `		SyBlobFormat(&(*pOut),"string(%u) \"",SyBlobLength(&pObj->sBlob));` |
|     5527 | 2634 | `		if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     5145 | 2635 | `			SyBlobAppend(&(*pOut),SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|     2568 | 2636 | `		}` |
|     5527 | 2637 | `		SyBlobAppend(&(*pOut),"\"\n",sizeof("\"\n")-1);` |
|     5527 | 2638 | `		return SXRET_OK;` |
|        - | 2639 | `	}` |
|      ! 0 | 2640 | `	if( pObj->iFlags & MEMOBJ_RES ){` |
|        - | 2641 | ``		/* php: `resource(N) of type (stream)`, and `(Unknown)` once closed —`` |
|        - | 2642 | `		 * which is exactly what PH7_VfsResourceType() already reports. The old` |
|        - | 2643 | `		 * shape printed the heap pointer through the string cast instead. */` |
|      ! 0 | 2644 | `		SyBlobFormat(&(*pOut),"resource(%u) of type (%s)\n",` |
|      ! 0 | 2645 | `			PH7_VmResourceId(pObj->pVm,pObj->x.pOther),` |
|      ! 0 | 2646 | `			PH7_VfsResourceType(pObj->x.pOther));` |
|      ! 0 | 2647 | `		return SXRET_OK;` |
|        - | 2648 | `	}` |
|        - | 2649 | ``	/* Anything else: the legacy `type(value)` shape. */`` |
|        - | 2650 | `	{` |
|      ! 0 | 2651 | `		const char *zType = PH7_MemObjTypeDump(pObj);` |
|      ! 0 | 2652 | `		SyBlobAppend(&(*pOut),zType,SyStrlen(zType));` |
|      ! 0 | 2653 | `		SyBlobAppend(&(*pOut),"(",sizeof(char));` |
|      ! 0 | 2654 | `		MemObjStringValue(&(*pOut),&(*pObj),FALSE);` |
|      ! 0 | 2655 | `		SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);` |
|        - | 2656 | `	}` |
|      ! 0 | 2657 | `	return rc;` |
|     9270 | 2658 | `}` |
|        - | 2659 |  |
