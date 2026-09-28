# src/ph7/builtin_bcmath.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1514/1695 lines (89.32%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `/*` |
|      - |    7 | ` * Section:` |
|      - |    8 | ` *    ext/bcmath: arbitrary-precision DECIMAL arithmetic over strings.` |
|      - |    9 | ` * Status:` |
|      - |   10 | ` *    Stable.` |
|      - |   11 | ` *` |
|      - |   12 | ` * php's ext/bcmath carries libbcmath, which is GNU bc's arithmetic under the` |
|      - |   13 | ` * LGPL. PHL is BSD-3-Clause, so this is a RE-DERIVATION from the behaviour the` |
|      - |   14 | ` * PHP surface shows and not a port: the arithmetic below is ordinary schoolbook` |
|      - |   15 | ` * decimal arithmetic, and every rule that is a CHOICE rather than a consequence` |
|      - |   16 | ` * was measured against php 8.5.9 first. The choices, all of them observable:` |
|      - |   17 | ` *` |
|      - |   18 | ` *   - a bc number is a SIGN, a run of integer digits and a run of fraction` |
|      - |   19 | ` *     digits, and its SCALE (that fraction run's length) is part of its` |
|      - |   20 | ` *     identity: "1.500" is a different number from "1.5" to bcdiv() and to` |
|      - |   21 | ` *     BcMath\Number, and the same one to bccomp().` |
|      - |   22 | ` *   - every function computes the EXACT result and then TRUNCATES it toward` |
|      - |   23 | ` *     zero to the requested scale, padding with '0' when the exact answer is` |
|      - |   24 | `` *     shorter. Nothing here rounds: `bcmul('1.5','2.25',2)` is '3.37', and`` |
|      - |   25 | `` *     `bcpow('1.5','10',2)` is '57.66' -- the exact 57.6650390625 cut, not a`` |
|      - |   26 | ` *     chain of truncated squarings, which would answer '57.60'.` |
|      - |   27 | `` *   - the grammar is `[+-]? DIGIT* ('.' DIGIT*)?` and NOTHING else: no`` |
|      - |   28 | ` *     exponent, no space, no separator, no hex. The empty string, "+", "-" and` |
|      - |   29 | ` *     "." are all VALID and all mean zero. The scan stops at a NUL byte and` |
|      - |   30 | ` *     ignores whatever follows it, because php's does.` |
|      - |   31 | ` *   - the sign of zero is dropped on the way out: -0.0001 truncated to three` |
|      - |   32 | ` *     places is '0.000', never '-0.000'.` |
|      - |   33 | `` *   - the default scale is the `bcmath.scale` ini directive, which bcscale()`` |
|      - |   34 | ` *     reads and writes -- one slot, not two.` |
|      - |   35 | ` *` |
|      - |   36 | ` * The digits are held one per byte, most significant first, as values 0..9.` |
|      - |   37 | ` * That is not how a speed-first implementation would carry them (four or nine` |
|      - |   38 | ` * digits per limb is), and it is what keeps every routine below readable enough` |
|      - |   39 | ` * to check against the oracle by eye.` |
|      - |   40 | ` */` |
|      - |   41 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   42 | `/*` |
|      - |   43 | ` * The largest number of digits one value may hold. php has no such limit: it` |
|      - |   44 | ` * asks the allocator and dies with "Out of memory" (or, past a point,` |
|      - |   45 | ` * "Possible integer overflow in memory allocation") when the request cannot be` |
|      - |   46 | `` * met, which is what `bcpow('2','10000000000')` does there. PHL answers the`` |
|      - |   47 | ` * same SHAPE of failure -- a memory error, not a wrong number -- but decides it` |
|      - |   48 | ` * from a cap rather than by asking a 30GB box for 240GB first. No reachable` |
|      - |   49 | ` * program computes with 32 million digits; every one that tries fails in php too.` |
|      - |   50 | ` */` |
|      - |   51 | `#define BC_MAX_DIGITS 33554432u` |
|      - |   52 | `/* php's cap on every $scale argument, spelled verbatim in its messages. */` |
|      - |   53 | `#define BC_MAX_SCALE  2147483647` |
|      - |   54 |  |
|      - |   55 | `/*` |
|      - |   56 | `` * One bc number: a sign, `nInt` integer digits and `nFrac` fraction digits, in`` |
|      - |   57 | ` * ONE array, most significant first, one digit (0..9) per byte.` |
|      - |   58 | ` *` |
|      - |   59 | ` * NORMALIZED means nInt >= 1 with no leading zero unless the whole integer part` |
|      - |   60 | ` * IS zero (nInt == 1, digit 0), and bNeg == 0 whenever every digit is 0.` |
|      - |   61 | ` * Trailing fraction zeros are never trimmed: they are the number's scale.` |
|      - |   62 | ` */` |
|      - |   63 | `typedef struct BcNum BcNum;` |
|      - |   64 | `struct BcNum` |
|      - |   65 | `{` |
|      - |   66 | `	SyMemBackend *pAlloc; /* The VM allocator these digits came from */` |
|      - |   67 | `	unsigned char *aDig;  /* nInt + nFrac digits, MSD first; 0 while empty */` |
|      - |   68 | `	sxu32 nAlloc;         /* Digits the buffer can hold */` |
|      - |   69 | `	sxu32 nInt;           /* Integer digits (>= 1 once initialized) */` |
|      - |   70 | `	sxu32 nFrac;          /* Fraction digits, i.e. the SCALE */` |
|      - |   71 | `	int bNeg;             /* 1 when negative */` |
|      - |   72 | `};` |
|      - |   73 |  |
|   6096 |   74 | `static void BcNumInit(BcNum *p,SyMemBackend *pAlloc)` |
|      2 |   75 | `{` |
|   6098 |   76 | `	p->pAlloc = pAlloc;` |
|   6098 |   77 | `	p->aDig = 0;` |
|   6098 |   78 | `	p->nAlloc = p->nInt = p->nFrac = 0;` |
|   6098 |   79 | `	p->bNeg = 0;` |
|   6098 |   80 | `}` |
|   6096 |   81 | `static void BcNumRelease(BcNum *p)` |
|      2 |   82 | `{` |
|   6098 |   83 | `	if( p->aDig ){` |
|   4624 |   84 | `		SyMemBackendFree(p->pAlloc,p->aDig);` |
|   4624 |   85 | `		p->aDig = 0;` |
|   2311 |   86 | `	}` |
|   6098 |   87 | `	p->nAlloc = p->nInt = p->nFrac = 0;` |
|   6098 |   88 | `	p->bNeg = 0;` |
|   6098 |   89 | `}` |
|      - |   90 | `/*` |
|      - |   91 | ` * Give p room for exactly nInt+nFrac digits, all zero, and record the split.` |
|      - |   92 | ` * Answers -1 for a request past the cap or an allocation the backend refused --` |
|      - |   93 | ` * the two failures every caller propagates as a memory error.` |
|      - |   94 | ` */` |
|   9784 |   95 | `static int BcNumAlloc(BcNum *p,sxu32 nInt,sxu32 nFrac)` |
|      2 |   96 | `{` |
|      - |   97 | `	sxu32 nWant;` |
|   9786 |   98 | `	if( nInt < 1 ){` |
|    ! 0 |   99 | `		nInt = 1;` |
|    ! 0 |  100 | `	}` |
|   9784 |  101 | `	if( nInt > BC_MAX_DIGITS \|\| nFrac > BC_MAX_DIGITS` |
|   9786 |  102 | `	 \|\| nInt + nFrac > BC_MAX_DIGITS ){` |
|    ! 0 |  103 | `		return -1;` |
|      - |  104 | `	}` |
|   9786 |  105 | `	nWant = nInt + nFrac;` |
|   9786 |  106 | `	if( nWant > p->nAlloc ){` |
|   4954 |  107 | `		unsigned char *aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,nWant);` |
|   4954 |  108 | `		if( aNew == 0 ){` |
|    ! 0 |  109 | `			return -1;` |
|      - |  110 | `		}` |
|   4954 |  111 | `		if( p->aDig ){` |
|    331 |  112 | `			SyMemBackendFree(p->pAlloc,p->aDig);` |
|    165 |  113 | `		}` |
|   4954 |  114 | `		p->aDig = aNew;` |
|   4954 |  115 | `		p->nAlloc = nWant;` |
|   2476 |  116 | `	}` |
|   9786 |  117 | `	SyZero(p->aDig,nWant);` |
|   9786 |  118 | `	p->nInt = nInt;` |
|   9786 |  119 | `	p->nFrac = nFrac;` |
|   9786 |  120 | `	p->bNeg = 0;` |
|   9786 |  121 | `	return 0;` |
|   4894 |  122 | `}` |
|   1838 |  123 | `static int BcNumIsZero(const BcNum *p)` |
|      2 |  124 | `{` |
|   1840 |  125 | `	sxu32 i, n = p->nInt + p->nFrac;` |
|   2294 |  126 | `	for( i = 0 ; i < n ; ++i ){` |
|   2108 |  127 | `		if( p->aDig[i] != 0 ){` |
|   1654 |  128 | `			return 0;` |
|      - |  129 | `		}` |
|    229 |  130 | `	}` |
|    187 |  131 | `	return 1;` |
|    921 |  132 | `}` |
|      - |  133 | `/*` |
|      - |  134 | ` * Drop leading integer zeros (keeping one digit) and un-sign a zero. Every` |
|      - |  135 | ` * routine that writes digits ends here, which is what makes the compare, the` |
|      - |  136 | ` * output and the "is it zero" test above agree on one representation.` |
|      - |  137 | ` */` |
|  11356 |  138 | `static void BcNumNormalize(BcNum *p)` |
|      2 |  139 | `{` |
|  11358 |  140 | `	sxu32 nLead = 0;` |
|  24528 |  141 | `	while( nLead + 1 < p->nInt && p->aDig[nLead] == 0 ){` |
|  13172 |  142 | `		nLead++;` |
|      2 |  143 | `	}` |
|  11358 |  144 | `	if( nLead > 0 ){` |
|      - |  145 | `		/* Overlapping, and safe: the move is LEFTWARD and SyMemcpy copies front to` |
|      - |  146 | `		 * back, so every byte is read before the copy reaches its own slot. */` |
|   5088 |  147 | `		SyMemcpy(&p->aDig[nLead],p->aDig,p->nInt + p->nFrac - nLead);` |
|   5088 |  148 | `		p->nInt -= nLead;` |
|   2543 |  149 | `	}` |
|  11358 |  150 | `	if( p->bNeg && BcNumIsZero(p) ){` |
|     83 |  151 | `		p->bNeg = 0;` |
|     41 |  152 | `	}` |
|  11358 |  153 | `}` |
|      - |  154 | `/* Copy pSrc into pDest, which keeps its own buffer. */` |
|     86 |  155 | `static int BcNumCopy(BcNum *pDest,const BcNum *pSrc)` |
|      1 |  156 | `{` |
|     87 |  157 | `	if( BcNumAlloc(pDest,pSrc->nInt,pSrc->nFrac) ){` |
|    ! 0 |  158 | `		return -1;` |
|      - |  159 | `	}` |
|     87 |  160 | `	SyMemcpy(pSrc->aDig,pDest->aDig,pSrc->nInt + pSrc->nFrac);` |
|     87 |  161 | `	pDest->bNeg = pSrc->bNeg;` |
|     87 |  162 | `	return 0;` |
|     44 |  163 | `}` |
|      - |  164 | `/*` |
|      - |  165 | ` * php's number grammar, and the whole of it:` |
|      - |  166 | ` *` |
|      - |  167 | ` *     [+-]? DIGIT* ( '.' DIGIT* )?` |
|      - |  168 | ` *` |
|      - |  169 | ` * Every part is optional, so "", "+", "-", "." and "-." all parse to zero;` |
|      - |  170 | ` * "1." is 1 at scale 0 and ".5" is 5 at scale 1. Anything else -- a space at` |
|      - |  171 | ` * either end, an exponent, a second point, a separator, a NON-DIGIT byte -- is` |
|      - |  172 | `` * php's `is not well-formed`.`` |
|      - |  173 | ` *` |
|      - |  174 | ` * The scan stops at a NUL and ignores the rest of the buffer, which is php's` |
|      - |  175 | `` * own behaviour rather than an accident of C strings here: `bcadd("1\0" . "2",`` |
|      - |  176 | `` * "0")` is 1 there and refuses nothing.`` |
|      - |  177 | ` *` |
|      - |  178 | ` * Answers 1 for a parsed number, 0 for a malformed one and -1 for a memory` |
|      - |  179 | ` * failure.` |
|      - |  180 | ` */` |
|   2146 |  181 | `static int BcNumParse(BcNum *p,const char *zIn,int nIn)` |
|      2 |  182 | `{` |
|   2148 |  183 | `	const char *z = zIn, *zEnd;` |
|   2148 |  184 | `	const char *zInt, *zFrac = 0;` |
|   2148 |  185 | `	sxu32 nDigInt = 0, nDigFrac = 0;` |
|      - |  186 | `	sxu32 i;` |
|   2148 |  187 | `	int bNeg = 0;` |
|   2148 |  188 | `	if( nIn < 0 ){` |
|    ! 0 |  189 | `		nIn = 0;` |
|    ! 0 |  190 | `	}` |
|   2148 |  191 | `	zEnd = &zIn[nIn];` |
|  10910 |  192 | `	for( i = 0 ; i < (sxu32)nIn ; ++i ){` |
|   8768 |  193 | `		if( zIn[i] == 0 ){` |
|      5 |  194 | `			zEnd = &zIn[i];` |
|      5 |  195 | `			break;` |
|      - |  196 | `		}` |
|   4383 |  197 | `	}` |
|   2148 |  198 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|    275 |  199 | `		bNeg = (z[0] == '-');` |
|    275 |  200 | `		z++;` |
|    137 |  201 | `	}` |
|   2148 |  202 | `	zInt = z;` |
|   5370 |  203 | `	while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){` |
|   3224 |  204 | `		z++;` |
|      2 |  205 | `	}` |
|   2148 |  206 | `	nDigInt = (sxu32)(z - zInt);` |
|   2148 |  207 | `	if( z < zEnd && z[0] == '.' ){` |
|   1190 |  208 | `		z++;` |
|   1190 |  209 | `		zFrac = z;` |
|   5180 |  210 | `		while( z < zEnd && z[0] >= '0' && z[0] <= '9' ){` |
|   3992 |  211 | `			z++;` |
|      2 |  212 | `		}` |
|   1190 |  213 | `		nDigFrac = (sxu32)(z - zFrac);` |
|    594 |  214 | `	}` |
|   2148 |  215 | `	if( z != zEnd ){` |
|     55 |  216 | `		return 0; /* a byte the grammar has no place for */` |
|      - |  217 | `	}` |
|   2094 |  218 | `	if( BcNumAlloc(p,nDigInt < 1 ? 1 : nDigInt,nDigFrac) ){` |
|    ! 0 |  219 | `		return -1;` |
|      - |  220 | `	}` |
|   2094 |  221 | `	if( nDigInt > 0 ){` |
|   5272 |  222 | `		for( i = 0 ; i < nDigInt ; ++i ){` |
|   3210 |  223 | `			p->aDig[i] = (unsigned char)(zInt[i] - '0');` |
|   1606 |  224 | `		}` |
|   1031 |  225 | `	}` |
|   6082 |  226 | `	for( i = 0 ; i < nDigFrac ; ++i ){` |
|   3990 |  227 | `		p->aDig[p->nInt + i] = (unsigned char)(zFrac[i] - '0');` |
|   1996 |  228 | `	}` |
|   2094 |  229 | `	p->bNeg = bNeg;` |
|   2094 |  230 | `	BcNumNormalize(p);` |
|   2094 |  231 | `	return 1;` |
|   1075 |  232 | `}` |
|      - |  233 | ``/* The same, for an sxi64 -- BcMath\Number's `int` argument and operand. */`` |
|    108 |  234 | `static int BcNumFromInt64(BcNum *p,sxi64 iVal)` |
|      2 |  235 | `{` |
|      - |  236 | `	char zBuf[32];` |
|    110 |  237 | `	int n = 0;` |
|      - |  238 | `	sxu64 uVal;` |
|    110 |  239 | `	int bNeg = 0;` |
|    110 |  240 | `	if( iVal < 0 ){` |
|     22 |  241 | `		bNeg = 1;` |
|      - |  242 | `		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */` |
|     22 |  243 | `		uVal = (sxu64)0 - (sxu64)iVal;` |
|     12 |  244 | `	}else{` |
|     90 |  245 | `		uVal = (sxu64)iVal;` |
|      - |  246 | `	}` |
|     54 |  247 | `	do{` |
|    150 |  248 | `		zBuf[n++] = (char)('0' + (int)(uVal % 10));` |
|    150 |  249 | `		uVal /= 10;` |
|    150 |  250 | `	}while( uVal != 0 && n < (int)sizeof(zBuf) );` |
|    110 |  251 | `	if( BcNumAlloc(p,(sxu32)n,0) ){` |
|    ! 0 |  252 | `		return -1;` |
|      - |  253 | `	}` |
|      - |  254 | `	{` |
|      - |  255 | `		int i;` |
|    258 |  256 | `		for( i = 0 ; i < n ; ++i ){` |
|    150 |  257 | `			p->aDig[i] = (unsigned char)(zBuf[n - 1 - i] - '0');` |
|     76 |  258 | `		}` |
|      - |  259 | `	}` |
|    110 |  260 | `	p->bNeg = bNeg;` |
|    110 |  261 | `	BcNumNormalize(p);` |
|    110 |  262 | `	return 0;` |
|     56 |  263 | `}` |
|      - |  264 | `/*` |
|      - |  265 | ` * Render p the way php prints a bc number: an optional '-', the integer digits,` |
|      - |  266 | ` * and -- only when the scale is not zero -- a '.' and exactly nFrac digits.` |
|      - |  267 | ` */` |
|   1262 |  268 | `static int BcNumToBlob(const BcNum *p,SyBlob *pOut)` |
|      2 |  269 | `{` |
|      - |  270 | `	sxu32 i;` |
|   1264 |  271 | `	if( p->bNeg && !BcNumIsZero(p) ){` |
|    180 |  272 | `		if( SyBlobAppend(pOut,"-",sizeof(char)) != SXRET_OK ){` |
|    ! 0 |  273 | `			return -1;` |
|      - |  274 | `		}` |
|     89 |  275 | `	}` |
|   3452 |  276 | `	for( i = 0 ; i < p->nInt ; ++i ){` |
|   2190 |  277 | `		char c = (char)('0' + p->aDig[i]);` |
|   2190 |  278 | `		if( SyBlobAppend(pOut,&c,sizeof(char)) != SXRET_OK ){` |
|    ! 0 |  279 | `			return -1;` |
|      - |  280 | `		}` |
|   1096 |  281 | `	}` |
|   1264 |  282 | `	if( p->nFrac > 0 ){` |
|    638 |  283 | `		if( SyBlobAppend(pOut,".",sizeof(char)) != SXRET_OK ){` |
|    ! 0 |  284 | `			return -1;` |
|      - |  285 | `		}` |
|   3742 |  286 | `		for( i = 0 ; i < p->nFrac ; ++i ){` |
|   3106 |  287 | `			char c = (char)('0' + p->aDig[p->nInt + i]);` |
|   3106 |  288 | `			if( SyBlobAppend(pOut,&c,sizeof(char)) != SXRET_OK ){` |
|    ! 0 |  289 | `				return -1;` |
|      - |  290 | `			}` |
|   1554 |  291 | `		}` |
|    318 |  292 | `	}` |
|   1264 |  293 | `	return 0;` |
|    633 |  294 | `}` |
|      - |  295 | `/*` |
|      - |  296 | ` * Cut or pad p's fraction to exactly nScale digits.` |
|      - |  297 | ` *` |
|      - |  298 | ` * Cutting is php's TRUNCATION toward zero -- the digits are a MAGNITUDE and the` |
|      - |  299 | ` * sign lives beside them, so dropping the tail moves every number toward zero` |
|      - |  300 | ` * without a case for negatives. A value that becomes zero loses its sign here,` |
|      - |  301 | ` * which is why bcadd('-0.001','0',2) is '0.00'.` |
|      - |  302 | ` */` |
|    580 |  303 | `static int BcNumSetScale(BcNum *p,sxu32 nScale)` |
|      2 |  304 | `{` |
|    582 |  305 | `	if( nScale == p->nFrac ){` |
|    308 |  306 | `		return 0;` |
|      - |  307 | `	}` |
|    275 |  308 | `	if( nScale < p->nFrac ){` |
|     97 |  309 | `		p->nFrac = nScale;` |
|     49 |  310 | `	}else{` |
|    179 |  311 | `		sxu32 nAdd = nScale - p->nFrac;` |
|    179 |  312 | `		sxu32 nUsed = p->nInt + p->nFrac;` |
|    179 |  313 | `		if( nUsed + nAdd > BC_MAX_DIGITS ){` |
|    ! 0 |  314 | `			return -1;` |
|      - |  315 | `		}` |
|    179 |  316 | `		if( nUsed + nAdd > p->nAlloc ){` |
|    167 |  317 | `			unsigned char *aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,nUsed + nAdd);` |
|    167 |  318 | `			if( aNew == 0 ){` |
|    ! 0 |  319 | `				return -1;` |
|      - |  320 | `			}` |
|    167 |  321 | `			SyMemcpy(p->aDig,aNew,nUsed);` |
|    167 |  322 | `			SyMemBackendFree(p->pAlloc,p->aDig);` |
|    167 |  323 | `			p->aDig = aNew;` |
|    167 |  324 | `			p->nAlloc = nUsed + nAdd;` |
|     83 |  325 | `		}` |
|    179 |  326 | `		SyZero(&p->aDig[nUsed],nAdd);` |
|    179 |  327 | `		p->nFrac = nScale;` |
|      - |  328 | `	}` |
|    275 |  329 | `	BcNumNormalize(p);` |
|    275 |  330 | `	return 0;` |
|    292 |  331 | `}` |
|      - |  332 | `/*` |
|      - |  333 | `` * The digit at place `iPlace`, counting 0 for the units column, positive to the`` |
|      - |  334 | ` * left and negative to the right. Everything outside the number is a zero, which` |
|      - |  335 | ` * is what lets the magnitude routines below walk two numbers of different shapes` |
|      - |  336 | ` * with one loop.` |
|      - |  337 | ` */` |
|  20984 |  338 | `static int BcDigitAt(const BcNum *p,sxi64 iPlace)` |
|      2 |  339 | `{` |
|  20986 |  340 | `	sxi64 idx = (sxi64)p->nInt - 1 - iPlace;` |
|  20986 |  341 | `	if( idx < 0 \|\| idx >= (sxi64)(p->nInt + p->nFrac) ){` |
|   2486 |  342 | `		return 0;` |
|      - |  343 | `	}` |
|  18502 |  344 | `	return p->aDig[idx];` |
|  10494 |  345 | `}` |
|      - |  346 | `/* Compare MAGNITUDES: -1, 0 or 1, sign ignored. */` |
|    688 |  347 | `static int BcMagCmp(const BcNum *a,const BcNum *b)` |
|      2 |  348 | `{` |
|      - |  349 | `	sxi64 iHigh, iLow, i;` |
|    690 |  350 | `	iHigh = (sxi64)((a->nInt > b->nInt) ? a->nInt : b->nInt) - 1;` |
|    690 |  351 | `	iLow  = -(sxi64)((a->nFrac > b->nFrac) ? a->nFrac : b->nFrac);` |
|   2678 |  352 | `	for( i = iHigh ; i >= iLow ; --i ){` |
|   2572 |  353 | `		int da = BcDigitAt(a,i), db = BcDigitAt(b,i);` |
|   2572 |  354 | `		if( da != db ){` |
|    584 |  355 | `			return da < db ? -1 : 1;` |
|      - |  356 | `		}` |
|    996 |  357 | `	}` |
|    107 |  358 | `	return 0;` |
|    346 |  359 | `}` |
|      - |  360 | `/* Compare VALUES, php's bccomp answer before the scale cut. */` |
|    100 |  361 | `static int BcNumCmp(const BcNum *a,const BcNum *b)` |
|      2 |  362 | `{` |
|    102 |  363 | `	int za = BcNumIsZero(a), zb = BcNumIsZero(b);` |
|      - |  364 | `	int cmp;` |
|    102 |  365 | `	if( za && zb ){` |
|      5 |  366 | `		return 0;` |
|      - |  367 | `	}` |
|     98 |  368 | `	if( a->bNeg != b->bNeg ){` |
|      5 |  369 | `		return a->bNeg ? -1 : 1;` |
|      - |  370 | `	}` |
|     94 |  371 | `	cmp = BcMagCmp(a,b);` |
|     94 |  372 | `	return a->bNeg ? -cmp : cmp;` |
|     52 |  373 | `}` |
|      - |  374 | `/* pOut = \|a\| + \|b\|, unsigned. */` |
|    752 |  375 | `static int BcMagAdd(BcNum *pOut,const BcNum *a,const BcNum *b)` |
|      2 |  376 | `{` |
|    754 |  377 | `	sxu32 nInt = (a->nInt > b->nInt ? a->nInt : b->nInt) + 1;` |
|    754 |  378 | `	sxu32 nFrac = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;` |
|      - |  379 | `	sxi64 iHigh, iLow, i;` |
|    754 |  380 | `	int carry = 0;` |
|    754 |  381 | `	if( BcNumAlloc(pOut,nInt,nFrac) ){` |
|    ! 0 |  382 | `		return -1;` |
|      - |  383 | `	}` |
|    754 |  384 | `	iHigh = (sxi64)nInt - 1;` |
|    754 |  385 | `	iLow = -(sxi64)nFrac;` |
|   8274 |  386 | `	for( i = iLow ; i <= iHigh ; ++i ){` |
|   7522 |  387 | `		int sum = BcDigitAt(a,i) + BcDigitAt(b,i) + carry;` |
|   7522 |  388 | `		carry = sum / 10;` |
|   7522 |  389 | `		pOut->aDig[(sxu32)((sxi64)nInt - 1 - i)] = (unsigned char)(sum % 10);` |
|   3762 |  390 | `	}` |
|    754 |  391 | `	BcNumNormalize(pOut);` |
|    754 |  392 | `	return 0;` |
|    378 |  393 | `}` |
|      - |  394 | `/* pOut = \|a\| - \|b\|, and the caller has already checked \|a\| >= \|b\|. */` |
|    134 |  395 | `static int BcMagSub(BcNum *pOut,const BcNum *a,const BcNum *b)` |
|      2 |  396 | `{` |
|    136 |  397 | `	sxu32 nInt = a->nInt > b->nInt ? a->nInt : b->nInt;` |
|    136 |  398 | `	sxu32 nFrac = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;` |
|      - |  399 | `	sxi64 iHigh, iLow, i;` |
|    136 |  400 | `	int borrow = 0;` |
|    136 |  401 | `	if( BcNumAlloc(pOut,nInt,nFrac) ){` |
|    ! 0 |  402 | `		return -1;` |
|      - |  403 | `	}` |
|    136 |  404 | `	iHigh = (sxi64)nInt - 1;` |
|    136 |  405 | `	iLow = -(sxi64)nFrac;` |
|    538 |  406 | `	for( i = iLow ; i <= iHigh ; ++i ){` |
|    404 |  407 | `		int diff = BcDigitAt(a,i) - BcDigitAt(b,i) - borrow;` |
|    404 |  408 | `		if( diff < 0 ){` |
|    182 |  409 | `			diff += 10;` |
|    182 |  410 | `			borrow = 1;` |
|     92 |  411 | `		}else{` |
|    224 |  412 | `			borrow = 0;` |
|      - |  413 | `		}` |
|    404 |  414 | `		pOut->aDig[(sxu32)((sxi64)nInt - 1 - i)] = (unsigned char)diff;` |
|    203 |  415 | `	}` |
|    136 |  416 | `	BcNumNormalize(pOut);` |
|    136 |  417 | `	return 0;` |
|     69 |  418 | `}` |
|      - |  419 | `/*` |
|      - |  420 | ` * pOut = a + b and pOut = a - b, over the signs. Both write into a buffer that` |
|      - |  421 | ` * may not alias either operand, which every caller here honours.` |
|      - |  422 | ` */` |
|    276 |  423 | `static int BcNumAddSigned(BcNum *pOut,const BcNum *a,const BcNum *b,int bSubtract)` |
|      2 |  424 | `{` |
|    278 |  425 | `	int bNegB = bSubtract ? !b->bNeg : b->bNeg;` |
|      - |  426 | `	int rc;` |
|    278 |  427 | `	if( a->bNeg == bNegB ){` |
|    144 |  428 | `		rc = BcMagAdd(pOut,a,b);` |
|    144 |  429 | `		if( rc == 0 ){` |
|    144 |  430 | `			pOut->bNeg = a->bNeg;` |
|     71 |  431 | `		}` |
|     73 |  432 | `	}else{` |
|    136 |  433 | `		int cmp = BcMagCmp(a,b);` |
|    136 |  434 | `		if( cmp >= 0 ){` |
|     99 |  435 | `			rc = BcMagSub(pOut,a,b);` |
|     99 |  436 | `			if( rc == 0 ){` |
|     99 |  437 | `				pOut->bNeg = a->bNeg;` |
|     49 |  438 | `			}` |
|     50 |  439 | `		}else{` |
|     38 |  440 | `			rc = BcMagSub(pOut,b,a);` |
|     38 |  441 | `			if( rc == 0 ){` |
|     38 |  442 | `				pOut->bNeg = bNegB;` |
|     18 |  443 | `			}` |
|      - |  444 | `		}` |
|      - |  445 | `	}` |
|    278 |  446 | `	if( rc == 0 ){` |
|    278 |  447 | `		BcNumNormalize(pOut);` |
|    138 |  448 | `	}` |
|    278 |  449 | `	return rc;` |
|      2 |  450 | `}` |
|      - |  451 | `/*` |
|      - |  452 | ` * pOut = a * b, exact. The digit arrays multiply as plain integers -- schoolbook,` |
|      - |  453 | ` * one digit at a time -- and the point lands nFrac(a)+nFrac(b) places from the` |
|      - |  454 | ` * right, which is where the exact product's point always is.` |
|      - |  455 | ` */` |
|   1654 |  456 | `static int BcNumMul(BcNum *pOut,const BcNum *a,const BcNum *b)` |
|      1 |  457 | `{` |
|   1655 |  458 | `	sxu32 nA = a->nInt + a->nFrac, nB = b->nInt + b->nFrac;` |
|   1655 |  459 | `	sxu32 nFrac = a->nFrac + b->nFrac;` |
|      - |  460 | `	sxu32 nTotal, nInt;` |
|      - |  461 | `	sxu32 i, j;` |
|   1655 |  462 | `	if( nA > BC_MAX_DIGITS - nB ){` |
|    ! 0 |  463 | `		return -1;` |
|      - |  464 | `	}` |
|      - |  465 | `	/* nA >= 1 + a->nFrac and nB >= 1 + b->nFrac, so the product always has at` |
|      - |  466 | `	 * least two integer digits' worth of room -- nInt below cannot underflow. */` |
|   1655 |  467 | `	nTotal = nA + nB;` |
|   1655 |  468 | `	nInt = nTotal - nFrac;` |
|   1655 |  469 | `	if( BcNumAlloc(pOut,nInt,nFrac) ){` |
|    ! 0 |  470 | `		return -1;` |
|      - |  471 | `	}` |
|      - |  472 | `	/* aDig is nTotal digits wide now; multiply into it from the right. */` |
|   5321 |  473 | `	for( i = 0 ; i < nA ; ++i ){` |
|   3667 |  474 | `		int da = a->aDig[nA - 1 - i];` |
|   3667 |  475 | `		int carry = 0;` |
|   3667 |  476 | `		if( da == 0 ){` |
|    327 |  477 | `			continue;` |
|      - |  478 | `		}` |
|  23661 |  479 | `		for( j = 0 ; j < nB ; ++j ){` |
|  20321 |  480 | `			sxu32 k = nTotal - 1 - (i + j);` |
|  20321 |  481 | `			int prod = pOut->aDig[k] + da * b->aDig[nB - 1 - j] + carry;` |
|  20321 |  482 | `			pOut->aDig[k] = (unsigned char)(prod % 10);` |
|  20321 |  483 | `			carry = prod / 10;` |
|  10161 |  484 | `		}` |
|      - |  485 | `		{` |
|   3341 |  486 | `			sxu32 k = nTotal - 1 - (i + nB);` |
|   4225 |  487 | `			while( carry != 0 ){` |
|   1403 |  488 | `				int sum = pOut->aDig[k] + carry;` |
|   1403 |  489 | `				pOut->aDig[k] = (unsigned char)(sum % 10);` |
|   1403 |  490 | `				carry = sum / 10;` |
|   1403 |  491 | `				if( k == 0 ){` |
|    519 |  492 | `					break;` |
|      - |  493 | `				}` |
|    885 |  494 | `				k--;` |
|      1 |  495 | `			}` |
|      - |  496 | `		}` |
|   1671 |  497 | `	}` |
|   1655 |  498 | `	pOut->bNeg = (a->bNeg != b->bNeg);` |
|   1655 |  499 | `	BcNumNormalize(pOut);` |
|   1655 |  500 | `	return 0;` |
|    828 |  501 | `}` |
|      - |  502 | `/* ------------------------------------------------------------------ *` |
|      - |  503 | ` *  Division, and everything built on it                               *` |
|      - |  504 | ` * ------------------------------------------------------------------ */` |
|      - |  505 | `/* Trade two numbers' whole state, so a routine can hand its scratch result to` |
|      - |  506 | ` * its caller's slot without copying the digits. */` |
|   1864 |  507 | `static void BcNumSwap(BcNum *a,BcNum *b)` |
|      1 |  508 | `{` |
|   1865 |  509 | `	BcNum sTmp = *a;` |
|   1865 |  510 | `	*a = *b;` |
|   1865 |  511 | `	*b = sTmp;` |
|   1865 |  512 | `}` |
|      - |  513 | `/* Set p to a small non-negative integer (0..9 is all any caller needs). */` |
|    470 |  514 | `static int BcNumSmall(BcNum *p,int iVal)` |
|      1 |  515 | `{` |
|    471 |  516 | `	if( BcNumAlloc(p,1,0) ){` |
|    ! 0 |  517 | `		return -1;` |
|      - |  518 | `	}` |
|    471 |  519 | `	p->aDig[0] = (unsigned char)iVal;` |
|    471 |  520 | `	return 0;` |
|    236 |  521 | `}` |
|      - |  522 | `/*` |
|      - |  523 | ` * pOut = pIn's DIGITS with nAdd zeros appended, read as a scale-0 integer --` |
|      - |  524 | `` * i.e. the integer `pIn * 10^(pIn->nFrac + nAdd)`. This is how a scaled decimal`` |
|      - |  525 | ` * division is turned into an integer one.` |
|      - |  526 | ` */` |
|    308 |  527 | `static int BcNumShiftLeft(BcNum *pOut,const BcNum *pIn,sxu32 nAdd)` |
|      1 |  528 | `{` |
|    309 |  529 | `	sxu32 n = pIn->nInt + pIn->nFrac;` |
|    309 |  530 | `	if( nAdd > BC_MAX_DIGITS \|\| n > BC_MAX_DIGITS - nAdd ){` |
|    ! 0 |  531 | `		return -1;` |
|      - |  532 | `	}` |
|    309 |  533 | `	if( BcNumAlloc(pOut,n + nAdd,0) ){` |
|    ! 0 |  534 | `		return -1;` |
|      - |  535 | `	}` |
|    309 |  536 | `	SyMemcpy(pIn->aDig,pOut->aDig,n);` |
|      - |  537 | `	/* BcNumAlloc zeroed the buffer, so the nAdd tail digits are already '0'. */` |
|    309 |  538 | `	BcNumNormalize(pOut);` |
|    309 |  539 | `	return 0;` |
|    155 |  540 | `}` |
|      - |  541 | `/*` |
|      - |  542 | ` * pOut = pIn's DIGITS with the last nDrop dropped -- the integer floor of the` |
|      - |  543 | ` * magnitude divided by 10^nDrop, which is what a magnitude-plus-sign` |
|      - |  544 | ` * representation needs for a truncation toward zero.` |
|      - |  545 | ` */` |
|    422 |  546 | `static int BcNumShiftRight(BcNum *pOut,const BcNum *pIn,sxu32 nDrop)` |
|      1 |  547 | `{` |
|    423 |  548 | `	sxu32 n = pIn->nInt + pIn->nFrac;` |
|    423 |  549 | `	if( nDrop >= n ){` |
|     75 |  550 | `		return BcNumSmall(pOut,0);` |
|      - |  551 | `	}` |
|    349 |  552 | `	if( BcNumAlloc(pOut,n - nDrop,0) ){` |
|    ! 0 |  553 | `		return -1;` |
|      - |  554 | `	}` |
|    349 |  555 | `	SyMemcpy(pIn->aDig,pOut->aDig,n - nDrop);` |
|    349 |  556 | `	BcNumNormalize(pOut);` |
|    349 |  557 | `	return 0;` |
|    212 |  558 | `}` |
|      - |  559 | `/*` |
|      - |  560 | ` * Put the decimal point back into a scale-0 number: the last nScale digits` |
|      - |  561 | ` * become the fraction, with leading zeros prepended when there are not enough` |
|      - |  562 | ` * digits to reach it (1 / 10^5 comes back as five digits and needs a sixth).` |
|      - |  563 | ` */` |
|    464 |  564 | `static int BcNumSetPoint(BcNum *p,sxu32 nScale)` |
|      1 |  565 | `{` |
|    465 |  566 | `	sxu32 n = p->nInt + p->nFrac;` |
|    465 |  567 | `	if( n <= nScale ){` |
|     71 |  568 | `		sxu32 nPad = nScale + 1 - n;` |
|      - |  569 | `		unsigned char *aNew;` |
|      - |  570 | `		/* nScale runs to php's 2^31-1, so the cap has to be tested on the PAD` |
|      - |  571 | `		 * before the subtraction below can be trusted not to wrap. */` |
|     71 |  572 | `		if( nPad > BC_MAX_DIGITS \|\| n > BC_MAX_DIGITS - nPad ){` |
|    ! 0 |  573 | `			return -1;` |
|      - |  574 | `		}` |
|     71 |  575 | `		aNew = (unsigned char *)SyMemBackendAlloc(p->pAlloc,n + nPad);` |
|     71 |  576 | `		if( aNew == 0 ){` |
|    ! 0 |  577 | `			return -1;` |
|      - |  578 | `		}` |
|     71 |  579 | `		SyZero(aNew,nPad);` |
|     71 |  580 | `		SyMemcpy(p->aDig,&aNew[nPad],n);` |
|     71 |  581 | `		SyMemBackendFree(p->pAlloc,p->aDig);` |
|     71 |  582 | `		p->aDig = aNew;` |
|     71 |  583 | `		p->nAlloc = n + nPad;` |
|     71 |  584 | `		n += nPad;` |
|     35 |  585 | `	}` |
|    465 |  586 | `	p->nInt = n - nScale;` |
|    465 |  587 | `	p->nFrac = nScale;` |
|    465 |  588 | `	BcNumNormalize(p);` |
|    465 |  589 | `	return 0;` |
|    233 |  590 | `}` |
|      - |  591 | `/* aT[0..nD] = q * aD[0..nD-1], one digit longer than aD so the carry fits. */` |
|  57504 |  592 | `static void BcBufMulDigit(const unsigned char *aD,sxu32 nD,int q,unsigned char *aT)` |
|      1 |  593 | `{` |
|      - |  594 | `	sxu32 i;` |
|  57505 |  595 | `	int carry = 0;` |
| 969733 |  596 | `	for( i = 0 ; i < nD ; ++i ){` |
| 912229 |  597 | `		int prod = aD[nD - 1 - i] * q + carry;` |
| 912229 |  598 | `		aT[nD - i] = (unsigned char)(prod % 10);` |
| 912229 |  599 | `		carry = prod / 10;` |
| 456115 |  600 | `	}` |
|  57505 |  601 | `	aT[0] = (unsigned char)carry;` |
|  57505 |  602 | `}` |
|  51028 |  603 | `static int BcBufCmp(const unsigned char *a,const unsigned char *b,sxu32 n)` |
|      1 |  604 | `{` |
|      - |  605 | `	sxu32 i;` |
|  93799 |  606 | `	for( i = 0 ; i < n ; ++i ){` |
|  93651 |  607 | `		if( a[i] != b[i] ){` |
|  50881 |  608 | `			return a[i] < b[i] ? -1 : 1;` |
|      - |  609 | `		}` |
|  21386 |  610 | `	}` |
|    149 |  611 | `	return 0;` |
|  25515 |  612 | `}` |
|      - |  613 | `/* a -= b over equal-length buffers; the caller has checked a >= b. */` |
|   6476 |  614 | `static void BcBufSub(unsigned char *a,const unsigned char *b,sxu32 n)` |
|      1 |  615 | `{` |
|      - |  616 | `	sxu32 i;` |
|   6477 |  617 | `	int borrow = 0;` |
| 116049 |  618 | `	for( i = n ; i > 0 ; --i ){` |
| 109573 |  619 | `		int d = a[i-1] - b[i-1] - borrow;` |
| 109573 |  620 | `		if( d < 0 ){` |
|  41145 |  621 | `			d += 10;` |
|  41145 |  622 | `			borrow = 1;` |
|  20573 |  623 | `		}else{` |
|  68429 |  624 | `			borrow = 0;` |
|      - |  625 | `		}` |
| 109573 |  626 | `		a[i-1] = (unsigned char)d;` |
|  54787 |  627 | `	}` |
|   6477 |  628 | `}` |
|      - |  629 | `/*` |
|      - |  630 | ` * Long division over MAGNITUDES read as digit strings, the decimal point in` |
|      - |  631 | ` * neither operand consulted: pQ = \|pN\| / \|pD\| and pR = \|pN\| % \|pD\|, both scale 0` |
|      - |  632 | ` * and both non-negative. The caller has already refused a zero divisor.` |
|      - |  633 | ` *` |
|      - |  634 | ` * NEITHER output may alias an input: the first thing this does is reallocate` |
|      - |  635 | ` * pQ, which would free the digits it is about to read.` |
|      - |  636 | ` *` |
|      - |  637 | ` * One quotient digit per input digit, found by BINARY SEARCH over 0..9 (four` |
|      - |  638 | ` * trial multiplies) rather than by the leading-digit estimate a normalized` |
|      - |  639 | ` * Knuth division uses -- the estimate needs a normalization pass and a` |
|      - |  640 | ` * correction loop to be right, and at four trials a digit this is both exact by` |
|      - |  641 | ` * construction and fast enough for numbers a script hands in.` |
|      - |  642 | ` */` |
|   1916 |  643 | `static int BcMagDivMod(BcNum *pQ,BcNum *pR,const BcNum *pN,const BcNum *pD)` |
|      1 |  644 | `{` |
|   1917 |  645 | `	const unsigned char *zN = pN->aDig, *zD = pD->aDig;` |
|   1917 |  646 | `	sxu32 nN = pN->nInt + pN->nFrac, nD = pD->nInt + pD->nFrac;` |
|   1917 |  647 | `	unsigned char *aR = 0, *aT = 0;` |
|      - |  648 | `	sxu32 i;` |
|   1917 |  649 | `	int rc = -1;` |
|      - |  650 | `	/* The SIGNIFICANT width of each operand: 0.005 is the digit string "0005",` |
|      - |  651 | `	 * whose leading zeros are not part of the divisor. */` |
|   1961 |  652 | `	while( nD > 1 && zD[0] == 0 ){ zD++; nD--; }` |
|   1921 |  653 | `	while( nN > 1 && zN[0] == 0 ){ zN++; nN--; }` |
|   1917 |  654 | `	if( BcNumAlloc(pQ,nN,0) ){` |
|    ! 0 |  655 | `		return -1;` |
|      - |  656 | `	}` |
|   1917 |  657 | `	aR = (unsigned char *)SyMemBackendAlloc(pQ->pAlloc,nD + 1);` |
|   1917 |  658 | `	aT = (unsigned char *)SyMemBackendAlloc(pQ->pAlloc,nD + 1);` |
|   1917 |  659 | `	if( aR == 0 \|\| aT == 0 ){` |
|    ! 0 |  660 | `		goto out;` |
|      - |  661 | `	}` |
|   1917 |  662 | `	SyZero(aR,nD + 1);` |
|  18035 |  663 | `	for( i = 0 ; i < nN ; ++i ){` |
|  16119 |  664 | `		int lo = 0, hi = 9;` |
|      - |  665 | `		sxu32 k;` |
|      - |  666 | `		/* Shift the running remainder left one place and bring the next digit` |
|      - |  667 | `		 * down; aR stays nD+1 wide, which is the widest a remainder-plus-digit` |
|      - |  668 | `		 * can be. */` |
| 270709 |  669 | `		for( k = 0 ; k < nD ; ++k ){` |
| 254591 |  670 | `			aR[k] = aR[k + 1];` |
| 127296 |  671 | `		}` |
|  16119 |  672 | `		aR[nD] = zN[i];` |
|  67147 |  673 | `		while( lo < hi ){` |
|  51029 |  674 | `			int mid = (lo + hi + 1) / 2;` |
|  51029 |  675 | `			BcBufMulDigit(zD,nD,mid,aT);` |
|  51029 |  676 | `			if( BcBufCmp(aT,aR,nD + 1) <= 0 ){` |
|  12869 |  677 | `				lo = mid;` |
|   6435 |  678 | `			}else{` |
|  38161 |  679 | `				hi = mid - 1;` |
|      - |  680 | `			}` |
|      1 |  681 | `		}` |
|  16119 |  682 | `		if( lo > 0 ){` |
|   6477 |  683 | `			BcBufMulDigit(zD,nD,lo,aT);` |
|   6477 |  684 | `			BcBufSub(aR,aT,nD + 1);` |
|   3238 |  685 | `		}` |
|  16119 |  686 | `		pQ->aDig[i] = (unsigned char)lo;` |
|   8060 |  687 | `	}` |
|   1917 |  688 | `	if( BcNumAlloc(pR,nD + 1,0) ){` |
|    ! 0 |  689 | `		goto out;` |
|      - |  690 | `	}` |
|   1917 |  691 | `	SyMemcpy(aR,pR->aDig,nD + 1);` |
|   1917 |  692 | `	BcNumNormalize(pR);` |
|   1917 |  693 | `	BcNumNormalize(pQ);` |
|   1917 |  694 | `	rc = 0;` |
|    958 |  695 | `out:` |
|   1917 |  696 | `	if( aR ){ SyMemBackendFree(pQ->pAlloc,aR); }` |
|   1917 |  697 | `	if( aT ){ SyMemBackendFree(pQ->pAlloc,aT); }` |
|   1917 |  698 | `	return rc;` |
|    959 |  699 | `}` |
|      - |  700 | `/*` |
|      - |  701 | ` * pOut = a / b, truncated toward zero to nScale places. pOut may not alias` |
|      - |  702 | ` * either operand; the caller has already refused a zero divisor.` |
|      - |  703 | ` *` |
|      - |  704 | ` * The scaled quotient is an INTEGER one: with sa and sb the two scales,` |
|      - |  705 | ` *` |
|      - |  706 | ` *     trunc(a/b * 10^scale) = floor( (A * 10^(sb+scale)) / (B * 10^sa) )` |
|      - |  707 | ` *` |
|      - |  708 | ` * over the digit strings A and B, so only ONE of the two ever needs padding --` |
|      - |  709 | ` * whichever side the exponent sb+scale-sa falls on.` |
|      - |  710 | ` */` |
|    150 |  711 | `static int BcNumDivide(BcNum *pOut,const BcNum *a,const BcNum *b,sxu32 nScale)` |
|      1 |  712 | `{` |
|      - |  713 | `	BcNum sT, sR;` |
|    151 |  714 | `	const BcNum *pN = a, *pD = b;` |
|    151 |  715 | `	sxi64 e = (sxi64)b->nFrac + (sxi64)nScale - (sxi64)a->nFrac;` |
|    151 |  716 | `	int rc = -1;` |
|    151 |  717 | `	BcNumInit(&sT,a->pAlloc);` |
|    151 |  718 | `	BcNumInit(&sR,a->pAlloc);` |
|    151 |  719 | `	if( e > 0 ){` |
|     89 |  720 | `		if( e > (sxi64)BC_MAX_DIGITS \|\| BcNumShiftLeft(&sT,a,(sxu32)e) ){` |
|    ! 0 |  721 | `			goto out;` |
|      - |  722 | `		}` |
|     89 |  723 | `		pN = &sT;` |
|    107 |  724 | `	}else if( e < 0 ){` |
|     19 |  725 | `		if( -e > (sxi64)BC_MAX_DIGITS \|\| BcNumShiftLeft(&sT,b,(sxu32)(-e)) ){` |
|    ! 0 |  726 | `			goto out;` |
|      - |  727 | `		}` |
|     19 |  728 | `		pD = &sT;` |
|      9 |  729 | `	}` |
|    151 |  730 | `	if( BcMagDivMod(pOut,&sR,pN,pD) \|\| BcNumSetPoint(pOut,nScale) ){` |
|    ! 0 |  731 | `		goto out;` |
|      - |  732 | `	}` |
|    151 |  733 | `	pOut->bNeg = (a->bNeg != b->bNeg);` |
|    151 |  734 | `	BcNumNormalize(pOut);` |
|    151 |  735 | `	rc = 0;` |
|     75 |  736 | `out:` |
|    151 |  737 | `	BcNumRelease(&sT);` |
|    151 |  738 | `	BcNumRelease(&sR);` |
|    151 |  739 | `	return rc;` |
|      1 |  740 | `}` |
|      - |  741 | `/*` |
|      - |  742 | ` * php's divmod: the quotient is the TRUNCATED integer one (scale 0, whatever` |
|      - |  743 | `` * $scale says) and the remainder is `a - b * q` cut to $scale. Truncation is`` |
|      - |  744 | ` * what gives the remainder the sign of the DIVIDEND, both here and in bcmod():` |
|      - |  745 | ` * -10 % 3 is -1 and 10 % -3 is 1.` |
|      - |  746 | ` *` |
|      - |  747 | ` * pQ and pR may not alias the operands.` |
|      - |  748 | ` */` |
|     72 |  749 | `static int BcNumDivMod(BcNum *pQ,BcNum *pR,const BcNum *a,const BcNum *b,sxu32 nScale)` |
|      1 |  750 | `{` |
|      - |  751 | `	BcNum sT;` |
|     73 |  752 | `	int rc = -1;` |
|     73 |  753 | `	BcNumInit(&sT,a->pAlloc);` |
|     73 |  754 | `	if( BcNumDivide(pQ,a,b,0) ){` |
|    ! 0 |  755 | `		goto out;` |
|      - |  756 | `	}` |
|     73 |  757 | `	if( BcNumMul(&sT,b,pQ) ){` |
|    ! 0 |  758 | `		goto out;` |
|      - |  759 | `	}` |
|     73 |  760 | `	if( BcNumAddSigned(pR,a,&sT,1) \|\| BcNumSetScale(pR,nScale) ){` |
|    ! 0 |  761 | `		goto out;` |
|      - |  762 | `	}` |
|     73 |  763 | `	rc = 0;` |
|     36 |  764 | `out:` |
|     73 |  765 | `	BcNumRelease(&sT);` |
|     73 |  766 | `	return rc;` |
|      1 |  767 | `}` |
|      - |  768 | `/* p /= 2, in place, over a scale-0 magnitude. */` |
|    462 |  769 | `static void BcNumHalve(BcNum *p)` |
|      1 |  770 | `{` |
|    463 |  771 | `	sxu32 i, n = p->nInt + p->nFrac;` |
|    463 |  772 | `	int carry = 0;` |
|   6217 |  773 | `	for( i = 0 ; i < n ; ++i ){` |
|   5755 |  774 | `		int cur = carry * 10 + p->aDig[i];` |
|   5755 |  775 | `		p->aDig[i] = (unsigned char)(cur / 2);` |
|   5755 |  776 | `		carry = cur % 2;` |
|   2878 |  777 | `	}` |
|    463 |  778 | `	BcNumNormalize(p);` |
|    463 |  779 | `}` |
|      - |  780 | `/*` |
|      - |  781 | ` * pOut = floor(sqrt(pN)) over a scale-0 magnitude, by Newton's iteration` |
|      - |  782 | ` *` |
|      - |  783 | ` *     x <- (x + N/x) / 2` |
|      - |  784 | ` *` |
|      - |  785 | ` * started at 10^ceil(digits/2), which is above sqrt(N) for every N with that` |
|      - |  786 | ` * many digits. From above, the sequence decreases to floor(sqrt(N)) and then` |
|      - |  787 | ` * stops going down, which is the loop's exit test.` |
|      - |  788 | ` *` |
|      - |  789 | ` * Every step is a FULL-precision division, so the cost is about log2(scale)` |
|      - |  790 | ` * divisions -- fine to a few hundred places and visibly slower than php's` |
|      - |  791 | ` * limb-based one past a few thousand, which is the same trade the one-digit-per-` |
|      - |  792 | ` * byte representation makes everywhere else in this file.` |
|      - |  793 | ` */` |
|     76 |  794 | `static int BcIntSqrt(BcNum *pOut,const BcNum *pN)` |
|      1 |  795 | `{` |
|      - |  796 | `	BcNum sX, sQ, sR, sT;` |
|     77 |  797 | `	int rc = -1;` |
|     77 |  798 | `	if( BcNumIsZero(pN) ){` |
|      7 |  799 | `		return BcNumSmall(pOut,0);` |
|      - |  800 | `	}` |
|     71 |  801 | `	BcNumInit(&sX,pN->pAlloc);` |
|     71 |  802 | `	BcNumInit(&sQ,pN->pAlloc);` |
|     71 |  803 | `	BcNumInit(&sR,pN->pAlloc);` |
|     71 |  804 | `	BcNumInit(&sT,pN->pAlloc);` |
|     71 |  805 | `	if( BcNumSmall(&sT,1) \|\| BcNumShiftLeft(&sX,&sT,(pN->nInt + 1) / 2) ){` |
|    ! 0 |  806 | `		goto out;` |
|      - |  807 | `	}` |
|    231 |  808 | `	for( ;; ){` |
|    463 |  809 | `		if( BcNumIsZero(&sX) ){` |
|    ! 0 |  810 | `			break; /* unreachable for N >= 1; a guard, not a case */` |
|      - |  811 | `		}` |
|    463 |  812 | `		if( BcMagDivMod(&sQ,&sR,pN,&sX) ){` |
|    ! 0 |  813 | `			goto out;` |
|      - |  814 | `		}` |
|    463 |  815 | `		if( BcMagAdd(&sT,&sX,&sQ) ){` |
|    ! 0 |  816 | `			goto out;` |
|      - |  817 | `		}` |
|    463 |  818 | `		BcNumHalve(&sT);` |
|    463 |  819 | `		if( BcMagCmp(&sT,&sX) >= 0 ){` |
|     71 |  820 | `			break;` |
|      - |  821 | `		}` |
|    393 |  822 | `		BcNumSwap(&sX,&sT);` |
|      1 |  823 | `	}` |
|     71 |  824 | `	BcNumSwap(pOut,&sX);` |
|     71 |  825 | `	rc = 0;` |
|     35 |  826 | `out:` |
|     71 |  827 | `	BcNumRelease(&sX);` |
|     71 |  828 | `	BcNumRelease(&sQ);` |
|     71 |  829 | `	BcNumRelease(&sR);` |
|     71 |  830 | `	BcNumRelease(&sT);` |
|     71 |  831 | `	return rc;` |
|     39 |  832 | `}` |
|      - |  833 | `/*` |
|      - |  834 | ` * pOut = sqrt(a) truncated to nScale places. The whole job is one integer` |
|      - |  835 | ` * square root:` |
|      - |  836 | ` *` |
|      - |  837 | ` *     trunc(sqrt(a) * 10^scale) = floor( sqrt( A * 10^(2*scale - sa) ) )` |
|      - |  838 | ` *` |
|      - |  839 | ` * and when that exponent is NEGATIVE the digits are simply dropped first --` |
|      - |  840 | ` * floor(sqrt(x)) is floor(sqrt(floor(x))) for any x >= 0, so truncating the` |
|      - |  841 | ` * radicand cannot move the answer.` |
|      - |  842 | ` */` |
|     76 |  843 | `static int BcNumSqrt(BcNum *pOut,const BcNum *a,sxu32 nScale)` |
|      1 |  844 | `{` |
|      - |  845 | `	BcNum sN;` |
|     77 |  846 | `	sxi64 e = 2 * (sxi64)nScale - (sxi64)a->nFrac;` |
|     77 |  847 | `	int rc = -1;` |
|     77 |  848 | `	BcNumInit(&sN,a->pAlloc);` |
|     77 |  849 | `	if( e >= 0 ){` |
|     71 |  850 | `		if( e > (sxi64)BC_MAX_DIGITS \|\| BcNumShiftLeft(&sN,a,(sxu32)e) ){` |
|    ! 0 |  851 | `			goto out;` |
|      1 |  852 | `		}` |
|     42 |  853 | `	}else if( BcNumShiftRight(&sN,a,(sxu32)(-e)) ){` |
|    ! 0 |  854 | `		goto out;` |
|      - |  855 | `	}` |
|     77 |  856 | `	if( BcIntSqrt(pOut,&sN) \|\| BcNumSetPoint(pOut,nScale) ){` |
|    ! 0 |  857 | `		goto out;` |
|      - |  858 | `	}` |
|     77 |  859 | `	rc = 0;` |
|     38 |  860 | `out:` |
|     77 |  861 | `	BcNumRelease(&sN);` |
|     77 |  862 | `	return rc;` |
|      1 |  863 | `}` |
|      - |  864 | `/*` |
|      - |  865 | ` * pOut = a ** uExp, EXACT, by repeated squaring. Exact is the contract, not an` |
|      - |  866 | `` * implementation choice: `bcpow('1.5','10',2)` is '57.66', the exact`` |
|      - |  867 | ` * 57.6650390625 cut, where truncating each squaring to the scale would answer` |
|      - |  868 | ` * '57.60'.` |
|      - |  869 | ` */` |
|     70 |  870 | `static int BcNumPowInt(BcNum *pOut,const BcNum *a,sxu64 uExp)` |
|      1 |  871 | `{` |
|      - |  872 | `	BcNum sBase, sTmp;` |
|     71 |  873 | `	int rc = -1;` |
|     71 |  874 | `	BcNumInit(&sBase,a->pAlloc);` |
|     71 |  875 | `	BcNumInit(&sTmp,a->pAlloc);` |
|     71 |  876 | `	if( BcNumSmall(pOut,1) \|\| BcNumCopy(&sBase,a) ){` |
|    ! 0 |  877 | `		goto out;` |
|      - |  878 | `	}` |
|    243 |  879 | `	while( uExp != 0 ){` |
|    173 |  880 | `		if( uExp & 1 ){` |
|    113 |  881 | `			if( BcNumMul(&sTmp,pOut,&sBase) ){` |
|    ! 0 |  882 | `				goto out;` |
|      - |  883 | `			}` |
|    113 |  884 | `			BcNumSwap(pOut,&sTmp);` |
|     56 |  885 | `		}` |
|    173 |  886 | `		uExp >>= 1;` |
|    173 |  887 | `		if( uExp != 0 ){` |
|    109 |  888 | `			if( BcNumMul(&sTmp,&sBase,&sBase) ){` |
|    ! 0 |  889 | `				goto out;` |
|      - |  890 | `			}` |
|    109 |  891 | `			BcNumSwap(&sBase,&sTmp);` |
|     54 |  892 | `		}` |
|      1 |  893 | `	}` |
|     71 |  894 | `	rc = 0;` |
|     35 |  895 | `out:` |
|     71 |  896 | `	BcNumRelease(&sBase);` |
|     71 |  897 | `	BcNumRelease(&sTmp);` |
|     71 |  898 | `	return rc;` |
|      1 |  899 | `}` |
|      - |  900 | `/*` |
|      - |  901 | ` * The integer VALUE of a number whose fraction is all zeros, for the two` |
|      - |  902 | ` * arguments php reads as counts rather than as quantities (an exponent, a` |
|      - |  903 | ` * modulus). Answers 0 when the value does not fit an sxi64, which is what php` |
|      - |  904 | `` * calls `is too large`.`` |
|      - |  905 | ` */` |
|     78 |  906 | `static int BcNumToInt64(const BcNum *p,sxi64 *pOut)` |
|      1 |  907 | `{` |
|     79 |  908 | `	sxu64 uVal = 0;` |
|      - |  909 | `	sxu32 i;` |
|    245 |  910 | `	for( i = 0 ; i < p->nInt ; ++i ){` |
|      - |  911 | `		/* Screen BEFORE the multiply: past 2^63/10 the next step would wrap, and` |
|      - |  912 | `		 * a wrapped value is indistinguishable from a small one. */` |
|    171 |  913 | `		if( uVal > (sxu64)922337203685477580 ){` |
|      5 |  914 | `			return 0;` |
|      - |  915 | `		}` |
|    167 |  916 | `		uVal = uVal * 10 + p->aDig[i];` |
|    167 |  917 | `		if( uVal > (sxu64)0x8000000000000000 ){` |
|    ! 0 |  918 | `			return 0;` |
|      - |  919 | `		}` |
|     84 |  920 | `	}` |
|     75 |  921 | `	if( p->bNeg ){` |
|      - |  922 | `		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */` |
|     23 |  923 | `		*pOut = (sxi64)((sxu64)0 - uVal);` |
|     12 |  924 | `	}else{` |
|     53 |  925 | `		if( uVal > (sxu64)0x7FFFFFFFFFFFFFFF ){` |
|    ! 0 |  926 | `			return 0;` |
|      - |  927 | `		}` |
|     53 |  928 | `		*pOut = (sxi64)uVal;` |
|      - |  929 | `	}` |
|     75 |  930 | `	return 1;` |
|     40 |  931 | `}` |
|      - |  932 | `/* Does this number have a fraction at all? (Trailing zeros do not count: php` |
|      - |  933 | `` * accepts `bcpow('1','1.0')` and refuses `bcpow('1','1.5')`.) */`` |
|    230 |  934 | `static int BcNumHasFraction(const BcNum *p)` |
|      1 |  935 | `{` |
|      - |  936 | `	sxu32 i;` |
|    239 |  937 | `	for( i = 0 ; i < p->nFrac ; ++i ){` |
|     33 |  938 | `		if( p->aDig[p->nInt + i] != 0 ){` |
|     25 |  939 | `			return 1;` |
|      - |  940 | `		}` |
|      5 |  941 | `	}` |
|    207 |  942 | `	return 0;` |
|    116 |  943 | `}` |
|      - |  944 | `/* pOut = p's INTEGER part, sign kept -- the digits of a number whose fraction` |
|      - |  945 | ` * the caller has already established is all zeros. */` |
|     64 |  946 | `static int BcNumIntPart(BcNum *pOut,const BcNum *p)` |
|      1 |  947 | `{` |
|     65 |  948 | `	if( BcNumShiftRight(pOut,p,p->nFrac) ){` |
|    ! 0 |  949 | `		return -1;` |
|      - |  950 | `	}` |
|     65 |  951 | `	pOut->bNeg = p->bNeg;` |
|     65 |  952 | `	BcNumNormalize(pOut);` |
|     65 |  953 | `	return 0;` |
|     33 |  954 | `}` |
|      - |  955 | `/* pOut = (a * b) mod m over non-negative scale-0 magnitudes. pOut must alias` |
|      - |  956 | ` * none of the four other numbers. */` |
|   1272 |  957 | `static int BcModMul(BcNum *pOut,const BcNum *a,const BcNum *b,const BcNum *m,` |
|      - |  958 | `	BcNum *pTmp,BcNum *pQ)` |
|      1 |  959 | `{` |
|   1273 |  960 | `	if( BcNumMul(pTmp,a,b) ){` |
|    ! 0 |  961 | `		return -1;` |
|      - |  962 | `	}` |
|   1273 |  963 | `	pTmp->bNeg = 0;` |
|   1273 |  964 | `	return BcMagDivMod(pQ,pOut,pTmp,m);` |
|    637 |  965 | `}` |
|      - |  966 | `/*` |
|      - |  967 | ` * pOut = (a ** e) mod m, php's answer -- which is a TRUNCATED-division` |
|      - |  968 | ` * remainder, so the modulus's sign is ignored and the result carries the` |
|      - |  969 | ` * DIVIDEND's: the power's, i.e. the base's when the exponent is odd.` |
|      - |  970 | `` * `bcpowmod('-5','3','7')` is -6, not 1.`` |
|      - |  971 | ` *` |
|      - |  972 | ` * The exponent is consumed as DECIMAL DIGITS rather than as a machine integer,` |
|      - |  973 | ` * which is what lets php accept one of any width here where bcpow() refuses` |
|      - |  974 | `` * anything past a long: Horner in base ten, `res = res**10 * a**digit` for each`` |
|      - |  975 | ` * digit left to right, with a reduction after every multiply so nothing ever` |
|      - |  976 | ` * grows past the modulus. All three arguments are integers by now (php refuses` |
|      - |  977 | ` * a fractional one).` |
|      - |  978 | ` */` |
|     32 |  979 | `static int BcNumPowMod(BcNum *pOut,const BcNum *a,const BcNum *pExp,const BcNum *m)` |
|      1 |  980 | `{` |
|      - |  981 | `	BcNum sBase, sRes, sTmp, sQ, sM, sA, sB;` |
|      - |  982 | `	sxu32 i;` |
|      - |  983 | `	int bOddExp;` |
|     33 |  984 | `	int rc = -1;` |
|     33 |  985 | `	BcNumInit(&sBase,a->pAlloc);` |
|     33 |  986 | `	BcNumInit(&sRes,a->pAlloc);` |
|     33 |  987 | `	BcNumInit(&sTmp,a->pAlloc);` |
|     33 |  988 | `	BcNumInit(&sQ,a->pAlloc);` |
|     33 |  989 | `	BcNumInit(&sM,a->pAlloc);` |
|     33 |  990 | `	BcNumInit(&sA,a->pAlloc);` |
|     33 |  991 | `	BcNumInit(&sB,a->pAlloc);` |
|     33 |  992 | `	bOddExp = (pExp->aDig[pExp->nInt - 1] & 1) != 0;` |
|     33 |  993 | `	if( BcNumIntPart(&sM,m) \|\| BcNumIntPart(&sTmp,a) ){` |
|    ! 0 |  994 | `		goto out;` |
|      - |  995 | `	}` |
|     33 |  996 | `	sM.bNeg = sTmp.bNeg = 0;   /* magnitudes: the signs are decided at the end */` |
|     33 |  997 | `	if( BcNumSmall(&sRes,1) \|\| BcMagDivMod(&sQ,&sBase,&sTmp,&sM) ){` |
|    ! 0 |  998 | `		goto out;` |
|      - |  999 | `	}` |
|    165 | 1000 | `	for( i = 0 ; i < pExp->nInt ; ++i ){` |
|    133 | 1001 | `		int d = pExp->aDig[i];` |
|      - | 1002 | `		/* res <- res**10, as ((res**2)**2 * res)**2. */` |
|    132 | 1003 | `		if( BcModMul(&sA,&sRes,&sRes,&sM,&sTmp,&sQ)        /* res**2  */` |
|    132 | 1004 | `		 \|\| BcModMul(&sB,&sA,&sA,&sM,&sTmp,&sQ)            /* res**4  */` |
|    132 | 1005 | `		 \|\| BcModMul(&sA,&sB,&sRes,&sM,&sTmp,&sQ)          /* res**5  */` |
|    133 | 1006 | `		 \|\| BcModMul(&sRes,&sA,&sA,&sM,&sTmp,&sQ) ){       /* res**10 */` |
|    ! 0 | 1007 | `			goto out;` |
|      - | 1008 | `		}` |
|    877 | 1009 | `		while( d-- > 0 ){` |
|    745 | 1010 | `			if( BcModMul(&sA,&sRes,&sBase,&sM,&sTmp,&sQ) ){` |
|    ! 0 | 1011 | `				goto out;` |
|      - | 1012 | `			}` |
|    745 | 1013 | `			BcNumSwap(&sRes,&sA);` |
|      1 | 1014 | `		}` |
|     67 | 1015 | `	}` |
|     33 | 1016 | `	BcNumSwap(pOut,&sRes);` |
|     33 | 1017 | `	pOut->bNeg = (a->bNeg && bOddExp);` |
|     33 | 1018 | `	BcNumNormalize(pOut);` |
|     33 | 1019 | `	rc = 0;` |
|     16 | 1020 | `out:` |
|     33 | 1021 | `	BcNumRelease(&sBase);` |
|     33 | 1022 | `	BcNumRelease(&sRes);` |
|     33 | 1023 | `	BcNumRelease(&sTmp);` |
|     33 | 1024 | `	BcNumRelease(&sQ);` |
|     33 | 1025 | `	BcNumRelease(&sM);` |
|     33 | 1026 | `	BcNumRelease(&sA);` |
|     33 | 1027 | `	BcNumRelease(&sB);` |
|     33 | 1028 | `	return rc;` |
|      1 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | `` * The `bcmath.scale` directive, which is where every $scale argument defaults`` |
|      - | 1032 | ` * from and the only state bcscale() has. php clamps nothing here: a directive` |
|      - | 1033 | ` * outside 0..2147483647 is refused at the ini layer (see vm_builtin_ini.c), so` |
|      - | 1034 | ` * anything this reads back is already in range.` |
|      - | 1035 | ` */` |
|    178 | 1036 | `static sxu32 BcDefaultScale(ph7_vm *pVm)` |
|      1 | 1037 | `{` |
|    179 | 1038 | `	sxi64 iScale = PH7_VmIniGetInt(pVm,"bcmath.scale",0);` |
|    179 | 1039 | `	if( iScale < 0 ){` |
|    ! 0 | 1040 | `		return 0;` |
|      - | 1041 | `	}` |
|    179 | 1042 | `	if( iScale > BC_MAX_SCALE ){` |
|    ! 0 | 1043 | `		return (sxu32)BC_MAX_SCALE;` |
|      - | 1044 | `	}` |
|    179 | 1045 | `	return (sxu32)iScale;` |
|     90 | 1046 | `}` |
|      - | 1047 | `/*` |
|      - | 1048 | `` * Resolve a `?int $scale = null` argument: absent or null means the directive.`` |
|      - | 1049 | ` * Answers 0 having thrown php's ValueError when the value is out of range --` |
|      - | 1050 | ` * which names the ARGUMENT POSITION, so each caller passes its own.` |
|      - | 1051 | ` */` |
|    562 | 1052 | `static int BcArgScale(ph7_context *pCtx,int nArg,ph7_value **apArg,int iPos,` |
|      - | 1053 | `	const char *zFunc,sxu32 *pOut)` |
|      1 | 1054 | `{` |
|      - | 1055 | `	sxi64 iScale;` |
|    563 | 1056 | `	if( nArg <= iPos \|\| (apArg[iPos]->iFlags & MEMOBJ_NULL) ){` |
|    139 | 1057 | `		*pOut = BcDefaultScale(pCtx->pVm);` |
|    139 | 1058 | `		return 1;` |
|      - | 1059 | `	}` |
|    425 | 1060 | `	iScale = ph7_value_to_int64(apArg[iPos]);` |
|    425 | 1061 | `	if( iScale < 0 \|\| iScale > BC_MAX_SCALE ){` |
|     40 | 1062 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1063 | `			"%s(): Argument #%d ($scale) must be between 0 and %d",` |
|     13 | 1064 | `			zFunc,iPos + 1,BC_MAX_SCALE);` |
|     27 | 1065 | `		return 0;` |
|      - | 1066 | `	}` |
|    399 | 1067 | `	*pOut = (sxu32)iScale;` |
|    399 | 1068 | `	return 1;` |
|    282 | 1069 | `}` |
|      - | 1070 | `/*` |
|      - | 1071 | ` * Parse one string ARGUMENT into pNum, raising php's diagnostics: the ValueError` |
|      - | 1072 | ` * for a string the grammar refuses (named by position AND by parameter name,` |
|      - | 1073 | ` * which differ per function) and a memory error for a number too large to hold.` |
|      - | 1074 | ` */` |
|   1394 | 1075 | `static int BcArgNum(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zParam,` |
|      - | 1076 | `	const char *zFunc,BcNum *pNum)` |
|      1 | 1077 | `{` |
|      - | 1078 | `	const char *zStr;` |
|   1395 | 1079 | `	int nStr = 0;` |
|      - | 1080 | `	int rc;` |
|   1395 | 1081 | `	zStr = ph7_value_to_string(pArg,&nStr);` |
|   1395 | 1082 | `	rc = BcNumParse(pNum,zStr,nStr);` |
|   1395 | 1083 | `	if( rc == 1 ){` |
|   1351 | 1084 | `		return 1;` |
|      - | 1085 | `	}` |
|     45 | 1086 | `	if( rc == 0 ){` |
|     67 | 1087 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1088 | `			"%s(): Argument #%d ($%s) is not well-formed",` |
|     22 | 1089 | `			zFunc,iPos + 1,zParam);` |
|     23 | 1090 | `	}else{` |
|    ! 0 | 1091 | `		PH7_ContextMemoryError(pCtx);` |
|      - | 1092 | `	}` |
|     45 | 1093 | `	return 0;` |
|    698 | 1094 | `}` |
|      - | 1095 | `/* Hand a finished number back as php's string answer. */` |
|    724 | 1096 | `static int BcResultNum(ph7_context *pCtx,const BcNum *pNum)` |
|      1 | 1097 | `{` |
|      - | 1098 | `	SyBlob sOut;` |
|      - | 1099 | `	int rc;` |
|    725 | 1100 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    725 | 1101 | `	rc = BcNumToBlob(pNum,&sOut);` |
|    725 | 1102 | `	if( rc == 0 ){` |
|    725 | 1103 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    362 | 1104 | `	}` |
|    725 | 1105 | `	SyBlobRelease(&sOut);` |
|    725 | 1106 | `	if( rc ){` |
|    ! 0 | 1107 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1108 | `	}` |
|    725 | 1109 | `	return PH7_OK;` |
|    363 | 1110 | `}` |
|      - | 1111 | `/*` |
|      - | 1112 | ` * bcadd/bcsub/bcmul share everything but one operation: parse both operands,` |
|      - | 1113 | ` * compute EXACTLY, then cut the answer to the scale.` |
|      - | 1114 | ` */` |
|      - | 1115 | `#define BC_OP_ADD 0` |
|      - | 1116 | `#define BC_OP_SUB 1` |
|      - | 1117 | `#define BC_OP_MUL 2` |
|    246 | 1118 | `static int BcBinaryOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iOp,` |
|      - | 1119 | `	const char *zFunc)` |
|      1 | 1120 | `{` |
|      - | 1121 | `	BcNum sA, sB, sR;` |
|    247 | 1122 | `	sxu32 nScale = 0;` |
|    247 | 1123 | `	int rc = PH7_OK;` |
|    247 | 1124 | `	int bOk = 0;` |
|    247 | 1125 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|    247 | 1126 | `	BcNumInit(&sB,&pCtx->pVm->sAllocator);` |
|    247 | 1127 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|      - | 1128 | `	/* php screens $scale FIRST, before it looks at either number:` |
|      - | 1129 | ``	 * `bcadd('x','1',-1)` names the SCALE. */`` |
|    246 | 1130 | `	if( !BcArgScale(pCtx,nArg,apArg,2,zFunc,&nScale)` |
|    241 | 1131 | `	 \|\| !BcArgNum(pCtx,apArg[0],0,"num1",zFunc,&sA)` |
|    225 | 1132 | `	 \|\| !BcArgNum(pCtx,apArg[1],1,"num2",zFunc,&sB) ){` |
|     41 | 1133 | `		goto done;` |
|      - | 1134 | `	}` |
|    207 | 1135 | `	if( iOp == BC_OP_MUL ){` |
|     67 | 1136 | `		bOk = BcNumMul(&sR,&sA,&sB) == 0;` |
|     34 | 1137 | `	}else{` |
|    141 | 1138 | `		bOk = BcNumAddSigned(&sR,&sA,&sB,iOp == BC_OP_SUB) == 0;` |
|      - | 1139 | `	}` |
|    207 | 1140 | `	if( !bOk \|\| BcNumSetScale(&sR,nScale) ){` |
|    ! 0 | 1141 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1142 | `		goto done;` |
|      - | 1143 | `	}` |
|    207 | 1144 | `	rc = BcResultNum(pCtx,&sR);` |
|    123 | 1145 | `done:` |
|    247 | 1146 | `	BcNumRelease(&sA);` |
|    247 | 1147 | `	BcNumRelease(&sB);` |
|    247 | 1148 | `	BcNumRelease(&sR);` |
|    247 | 1149 | `	return rc;` |
|      1 | 1150 | `}` |
|      - | 1151 | `/*` |
|      - | 1152 | ` * string bcadd(string $num1, string $num2, ?int $scale = null)` |
|      - | 1153 | ` */` |
|    136 | 1154 | `PH7_PRIVATE int PH7_builtin_bcadd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1155 | `{` |
|    137 | 1156 | `	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_ADD,"bcadd");` |
|      1 | 1157 | `}` |
|      - | 1158 | `/*` |
|      - | 1159 | ` * string bcsub(string $num1, string $num2, ?int $scale = null)` |
|      - | 1160 | ` */` |
|     40 | 1161 | `PH7_PRIVATE int PH7_builtin_bcsub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1162 | `{` |
|     41 | 1163 | `	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_SUB,"bcsub");` |
|      1 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ` * string bcmul(string $num1, string $num2, ?int $scale = null)` |
|      - | 1167 | ` */` |
|     70 | 1168 | `PH7_PRIVATE int PH7_builtin_bcmul(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1169 | `{` |
|     71 | 1170 | `	return BcBinaryOp(pCtx,nArg,apArg,BC_OP_MUL,"bcmul");` |
|      1 | 1171 | `}` |
|      - | 1172 | `/*` |
|      - | 1173 | ` * int bccomp(string $num1, string $num2, ?int $scale = null)` |
|      - | 1174 | ` *` |
|      - | 1175 | ` * The scale is a CUT before the comparison, not a tolerance: bccomp('1.1','1.2')` |
|      - | 1176 | ` * at the default scale of 0 compares 1 against 1 and answers 0.` |
|      - | 1177 | ` */` |
|     70 | 1178 | `PH7_PRIVATE int PH7_builtin_bccomp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1179 | `{` |
|      - | 1180 | `	BcNum sA, sB;` |
|     71 | 1181 | `	sxu32 nScale = 0;` |
|     71 | 1182 | `	int rc = PH7_OK;` |
|     71 | 1183 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     71 | 1184 | `	BcNumInit(&sB,&pCtx->pVm->sAllocator);` |
|     70 | 1185 | `	if( !BcArgScale(pCtx,nArg,apArg,2,"bccomp",&nScale)` |
|     68 | 1186 | `	 \|\| !BcArgNum(pCtx,apArg[0],0,"num1","bccomp",&sA)` |
|     66 | 1187 | `	 \|\| !BcArgNum(pCtx,apArg[1],1,"num2","bccomp",&sB) ){` |
|      9 | 1188 | `		goto done;` |
|      - | 1189 | `	}` |
|     63 | 1190 | `	if( BcNumSetScale(&sA,nScale) \|\| BcNumSetScale(&sB,nScale) ){` |
|    ! 0 | 1191 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1192 | `		goto done;` |
|      - | 1193 | `	}` |
|     63 | 1194 | `	ph7_result_int(pCtx,BcNumCmp(&sA,&sB));` |
|     35 | 1195 | `done:` |
|     71 | 1196 | `	BcNumRelease(&sA);` |
|     71 | 1197 | `	BcNumRelease(&sB);` |
|     71 | 1198 | `	return rc;` |
|      1 | 1199 | `}` |
|      - | 1200 | `/*` |
|      - | 1201 | ` * bcdiv/bcmod/bcdivmod share their whole argument shape and differ only in what` |
|      - | 1202 | ` * they hand back, so one body serves all three. php's ZERO-divisor refusal is` |
|      - | 1203 | ` * worded from the OPERATION rather than from the function: bcdivmod() says` |
|      - | 1204 | ` * "Division by zero" where bcmod() says "Modulo by zero".` |
|      - | 1205 | ` */` |
|      - | 1206 | `#define BC_DIV_QUOTIENT 0` |
|      - | 1207 | `#define BC_DIV_MODULUS  1` |
|      - | 1208 | `#define BC_DIV_BOTH     2` |
|     88 | 1209 | `static int BcDivideOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iWhat,` |
|      - | 1210 | `	const char *zFunc)` |
|      1 | 1211 | `{` |
|      - | 1212 | `	BcNum sA, sB, sQ, sR;` |
|     89 | 1213 | `	sxu32 nScale = 0;` |
|     89 | 1214 | `	int rc = PH7_OK;` |
|     89 | 1215 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     89 | 1216 | `	BcNumInit(&sB,&pCtx->pVm->sAllocator);` |
|     89 | 1217 | `	BcNumInit(&sQ,&pCtx->pVm->sAllocator);` |
|     89 | 1218 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     88 | 1219 | `	if( !BcArgScale(pCtx,nArg,apArg,2,zFunc,&nScale)` |
|     85 | 1220 | `	 \|\| !BcArgNum(pCtx,apArg[0],0,"num1",zFunc,&sA)` |
|     83 | 1221 | `	 \|\| !BcArgNum(pCtx,apArg[1],1,"num2",zFunc,&sB) ){` |
|      7 | 1222 | `		goto done;` |
|      - | 1223 | `	}` |
|     83 | 1224 | `	if( BcNumIsZero(&sB) ){` |
|     19 | 1225 | `		PH7_VmThrowException(pCtx,"DivisionByZeroError",` |
|      6 | 1226 | `			iWhat == BC_DIV_MODULUS ? "Modulo by zero" : "Division by zero");` |
|     13 | 1227 | `		goto done;` |
|      - | 1228 | `	}` |
|     71 | 1229 | `	if( iWhat == BC_DIV_QUOTIENT ){` |
|     31 | 1230 | `		if( BcNumDivide(&sQ,&sA,&sB,nScale) ){` |
|    ! 0 | 1231 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1232 | `			goto done;` |
|      - | 1233 | `		}` |
|     31 | 1234 | `		rc = BcResultNum(pCtx,&sQ);` |
|     31 | 1235 | `		goto done;` |
|      - | 1236 | `	}` |
|     41 | 1237 | `	if( BcNumDivMod(&sQ,&sR,&sA,&sB,nScale) ){` |
|    ! 0 | 1238 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1239 | `		goto done;` |
|      - | 1240 | `	}` |
|     41 | 1241 | `	if( iWhat == BC_DIV_MODULUS ){` |
|     21 | 1242 | `		rc = BcResultNum(pCtx,&sR);` |
|     21 | 1243 | `		goto done;` |
|      - | 1244 | `	}` |
|      - | 1245 | `	{` |
|      - | 1246 | `		/* bcdivmod answers the LIST php answers: the integer quotient, then the` |
|      - | 1247 | `		 * remainder at the scale. */` |
|     21 | 1248 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     21 | 1249 | `		ph7_value *pCur = ph7_context_new_scalar(pCtx);` |
|      - | 1250 | `		SyBlob sTxt;` |
|     21 | 1251 | `		if( pOut == 0 \|\| pCur == 0 ){` |
|    ! 0 | 1252 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1253 | `			goto done;` |
|      - | 1254 | `		}` |
|     21 | 1255 | `		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);` |
|     21 | 1256 | `		if( BcNumToBlob(&sQ,&sTxt) ){` |
|    ! 0 | 1257 | `			SyBlobRelease(&sTxt);` |
|    ! 0 | 1258 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1259 | `			goto done;` |
|      - | 1260 | `		}` |
|     21 | 1261 | `		ph7_value_string(pCur,(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));` |
|     21 | 1262 | `		ph7_array_add_elem(pOut,0,pCur);` |
|     21 | 1263 | `		SyBlobReset(&sTxt);` |
|     21 | 1264 | `		ph7_value_reset_string_cursor(pCur);` |
|     21 | 1265 | `		if( BcNumToBlob(&sR,&sTxt) ){` |
|    ! 0 | 1266 | `			SyBlobRelease(&sTxt);` |
|    ! 0 | 1267 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1268 | `			goto done;` |
|      - | 1269 | `		}` |
|     21 | 1270 | `		ph7_value_string(pCur,(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));` |
|     21 | 1271 | `		ph7_array_add_elem(pOut,0,pCur);` |
|     21 | 1272 | `		SyBlobRelease(&sTxt);` |
|     21 | 1273 | `		ph7_result_value(pCtx,pOut);` |
|     21 | 1274 | `		ph7_context_release_value(pCtx,pCur);` |
|     21 | 1275 | `		ph7_context_release_value(pCtx,pOut);` |
|     10 | 1276 | `	}` |
|     44 | 1277 | `done:` |
|     89 | 1278 | `	BcNumRelease(&sA);` |
|     89 | 1279 | `	BcNumRelease(&sB);` |
|     89 | 1280 | `	BcNumRelease(&sQ);` |
|     89 | 1281 | `	BcNumRelease(&sR);` |
|     89 | 1282 | `	return rc;` |
|      1 | 1283 | `}` |
|      - | 1284 | `/*` |
|      - | 1285 | ` * string bcdiv(string $num1, string $num2, ?int $scale = null)` |
|      - | 1286 | ` */` |
|     38 | 1287 | `PH7_PRIVATE int PH7_builtin_bcdiv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1288 | `{` |
|     39 | 1289 | `	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_QUOTIENT,"bcdiv");` |
|      1 | 1290 | `}` |
|      - | 1291 | `/*` |
|      - | 1292 | ` * string bcmod(string $num1, string $num2, ?int $scale = null)` |
|      - | 1293 | ` */` |
|     26 | 1294 | `PH7_PRIVATE int PH7_builtin_bcmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1295 | `{` |
|     27 | 1296 | `	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_MODULUS,"bcmod");` |
|      1 | 1297 | `}` |
|      - | 1298 | `/*` |
|      - | 1299 | ` * array bcdivmod(string $num1, string $num2, ?int $scale = null)` |
|      - | 1300 | ` */` |
|     24 | 1301 | `PH7_PRIVATE int PH7_builtin_bcdivmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1302 | `{` |
|     25 | 1303 | `	return BcDivideOp(pCtx,nArg,apArg,BC_DIV_BOTH,"bcdivmod");` |
|      1 | 1304 | `}` |
|      - | 1305 | `/*` |
|      - | 1306 | ` * string bcpow(string $num, string $exponent, ?int $scale = null)` |
|      - | 1307 | ` *` |
|      - | 1308 | ` * The exponent is a COUNT, so php refuses a fractional one (a zero fraction is` |
|      - | 1309 | ` * fine: '1.0' is the integer 1) and refuses one no long can hold. A negative` |
|      - | 1310 | ` * exponent is the reciprocal of the exact power, divided at the scale -- and` |
|      - | 1311 | ` * over a zero base that is php's DivisionByZeroError rather than a ValueError.` |
|      - | 1312 | ` */` |
|     54 | 1313 | `PH7_PRIVATE int PH7_builtin_bcpow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1314 | `{` |
|      - | 1315 | `	BcNum sA, sE, sP, sOne;` |
|     55 | 1316 | `	sxu32 nScale = 0;` |
|     55 | 1317 | `	sxi64 iExp = 0;` |
|     55 | 1318 | `	int rc = PH7_OK;` |
|     55 | 1319 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     55 | 1320 | `	BcNumInit(&sE,&pCtx->pVm->sAllocator);` |
|     55 | 1321 | `	BcNumInit(&sP,&pCtx->pVm->sAllocator);` |
|     55 | 1322 | `	BcNumInit(&sOne,&pCtx->pVm->sAllocator);` |
|     54 | 1323 | `	if( !BcArgScale(pCtx,nArg,apArg,2,"bcpow",&nScale)` |
|     53 | 1324 | `	 \|\| !BcArgNum(pCtx,apArg[0],0,"num","bcpow",&sA)` |
|     52 | 1325 | `	 \|\| !BcArgNum(pCtx,apArg[1],1,"exponent","bcpow",&sE) ){` |
|      5 | 1326 | `		goto done;` |
|      - | 1327 | `	}` |
|     51 | 1328 | `	if( BcNumHasFraction(&sE) ){` |
|      5 | 1329 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1330 | `			"bcpow(): Argument #2 ($exponent) cannot have a fractional part");` |
|      5 | 1331 | `		goto done;` |
|      - | 1332 | `	}` |
|     47 | 1333 | `	if( !BcNumToInt64(&sE,&iExp) ){` |
|      5 | 1334 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1335 | `			"bcpow(): Argument #2 ($exponent) is too large");` |
|      5 | 1336 | `		goto done;` |
|      - | 1337 | `	}` |
|     43 | 1338 | `	if( iExp < 0 && BcNumIsZero(&sA) ){` |
|      3 | 1339 | `		PH7_VmThrowException(pCtx,"DivisionByZeroError","Negative power of zero");` |
|      3 | 1340 | `		goto done;` |
|      - | 1341 | `	}` |
|      - | 1342 | `	{` |
|      - | 1343 | `		/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */` |
|     41 | 1344 | `		sxu64 uExp = iExp < 0 ? ((sxu64)0 - (sxu64)iExp) : (sxu64)iExp;` |
|     41 | 1345 | `		if( BcNumPowInt(&sP,&sA,uExp) ){` |
|    ! 0 | 1346 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1347 | `			goto done;` |
|      - | 1348 | `		}` |
|      - | 1349 | `	}` |
|     41 | 1350 | `	if( iExp < 0 ){` |
|      9 | 1351 | `		if( BcNumSmall(&sOne,1) \|\| BcNumDivide(&sA,&sOne,&sP,nScale) ){` |
|    ! 0 | 1352 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1353 | `			goto done;` |
|      - | 1354 | `		}` |
|      9 | 1355 | `		rc = BcResultNum(pCtx,&sA);` |
|      9 | 1356 | `		goto done;` |
|      - | 1357 | `	}` |
|     33 | 1358 | `	if( BcNumSetScale(&sP,nScale) ){` |
|    ! 0 | 1359 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1360 | `		goto done;` |
|      - | 1361 | `	}` |
|     33 | 1362 | `	rc = BcResultNum(pCtx,&sP);` |
|     27 | 1363 | `done:` |
|     55 | 1364 | `	BcNumRelease(&sA);` |
|     55 | 1365 | `	BcNumRelease(&sE);` |
|     55 | 1366 | `	BcNumRelease(&sP);` |
|     55 | 1367 | `	BcNumRelease(&sOne);` |
|     55 | 1368 | `	return rc;` |
|      1 | 1369 | `}` |
|      - | 1370 | `/*` |
|      - | 1371 | ` * string bcpowmod(string $num, string $exponent, string $modulus, ?int $scale = null)` |
|      - | 1372 | ` *` |
|      - | 1373 | ` * All three are COUNTS here: php refuses a fractional part in any of them, and` |
|      - | 1374 | ` * a negative exponent (there is no modular inverse in this API).` |
|      - | 1375 | ` */` |
|     48 | 1376 | `PH7_PRIVATE int PH7_builtin_bcpowmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1377 | `{` |
|      - | 1378 | `	static const char *azParam[] = { "num", "exponent", "modulus" };` |
|      - | 1379 | `	BcNum aNum[3], sR;` |
|     49 | 1380 | `	sxu32 nScale = 0;` |
|     49 | 1381 | `	int rc = PH7_OK;` |
|      - | 1382 | `	int i;` |
|    193 | 1383 | `	for( i = 0 ; i < 3 ; ++i ){` |
|    145 | 1384 | `		BcNumInit(&aNum[i],&pCtx->pVm->sAllocator);` |
|     73 | 1385 | `	}` |
|     49 | 1386 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     49 | 1387 | `	if( !BcArgScale(pCtx,nArg,apArg,3,"bcpowmod",&nScale) ){` |
|      3 | 1388 | `		goto done;` |
|      - | 1389 | `	}` |
|    185 | 1390 | `	for( i = 0 ; i < 3 ; ++i ){` |
|    139 | 1391 | `		if( !BcArgNum(pCtx,apArg[i],i,azParam[i],"bcpowmod",&aNum[i]) ){` |
|    ! 0 | 1392 | `			goto done;` |
|      - | 1393 | `		}` |
|     70 | 1394 | `	}` |
|      - | 1395 | `	/* php's order, which a fuzz round found: the two leading arguments are` |
|      - | 1396 | `	 * screened for a fraction, then the EXPONENT's sign, and only then the` |
|      - | 1397 | `	 * modulus's fraction. A negative exponent beside a fractional modulus names` |
|      - | 1398 | `	 * the exponent. */` |
|    127 | 1399 | `	for( i = 0 ; i < 2 ; ++i ){` |
|     89 | 1400 | `		if( BcNumHasFraction(&aNum[i]) ){` |
|     13 | 1401 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1402 | `				"bcpowmod(): Argument #%d ($%s) cannot have a fractional part",` |
|      4 | 1403 | `				i + 1,azParam[i]);` |
|      9 | 1404 | `			goto done;` |
|      - | 1405 | `		}` |
|     41 | 1406 | `	}` |
|     39 | 1407 | `	if( aNum[1].bNeg && !BcNumIsZero(&aNum[1]) ){` |
|      7 | 1408 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1409 | `			"bcpowmod(): Argument #2 ($exponent) must be greater than or equal to 0");` |
|      7 | 1410 | `		goto done;` |
|      - | 1411 | `	}` |
|     33 | 1412 | `	if( BcNumHasFraction(&aNum[2]) ){` |
|      3 | 1413 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1414 | `			"bcpowmod(): Argument #3 ($modulus) cannot have a fractional part");` |
|      3 | 1415 | `		goto done;` |
|      - | 1416 | `	}` |
|     31 | 1417 | `	if( BcNumIsZero(&aNum[2]) ){` |
|      5 | 1418 | `		PH7_VmThrowException(pCtx,"DivisionByZeroError","Modulo by zero");` |
|      5 | 1419 | `		goto done;` |
|      - | 1420 | `	}` |
|     26 | 1421 | `	if( BcNumPowMod(&sR,&aNum[0],&aNum[1],&aNum[2])` |
|     27 | 1422 | `	 \|\| BcNumSetScale(&sR,nScale) ){` |
|    ! 0 | 1423 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1424 | `		goto done;` |
|      - | 1425 | `	}` |
|     27 | 1426 | `	rc = BcResultNum(pCtx,&sR);` |
|     24 | 1427 | `done:` |
|    193 | 1428 | `	for( i = 0 ; i < 3 ; ++i ){` |
|    145 | 1429 | `		BcNumRelease(&aNum[i]);` |
|     73 | 1430 | `	}` |
|     49 | 1431 | `	BcNumRelease(&sR);` |
|     49 | 1432 | `	return rc;` |
|      1 | 1433 | `}` |
|      - | 1434 | `/*` |
|      - | 1435 | ` * string bcsqrt(string $num, ?int $scale = null)` |
|      - | 1436 | ` */` |
|     56 | 1437 | `PH7_PRIVATE int PH7_builtin_bcsqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1438 | `{` |
|      - | 1439 | `	BcNum sA, sR;` |
|     57 | 1440 | `	sxu32 nScale = 0;` |
|     57 | 1441 | `	int rc = PH7_OK;` |
|     57 | 1442 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     57 | 1443 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     56 | 1444 | `	if( !BcArgScale(pCtx,nArg,apArg,1,"bcsqrt",&nScale)` |
|     56 | 1445 | `	 \|\| !BcArgNum(pCtx,apArg[0],0,"num","bcsqrt",&sA) ){` |
|      5 | 1446 | `		goto done;` |
|      - | 1447 | `	}` |
|     53 | 1448 | `	if( sA.bNeg && !BcNumIsZero(&sA) ){` |
|      3 | 1449 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1450 | `			"bcsqrt(): Argument #1 ($num) must be greater than or equal to 0");` |
|      3 | 1451 | `		goto done;` |
|      - | 1452 | `	}` |
|     51 | 1453 | `	if( BcNumSqrt(&sR,&sA,nScale) ){` |
|    ! 0 | 1454 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1455 | `		goto done;` |
|      - | 1456 | `	}` |
|     51 | 1457 | `	rc = BcResultNum(pCtx,&sR);` |
|     28 | 1458 | `done:` |
|     57 | 1459 | `	BcNumRelease(&sA);` |
|     57 | 1460 | `	BcNumRelease(&sR);` |
|     57 | 1461 | `	return rc;` |
|      1 | 1462 | `}` |
|      - | 1463 | `/* ------------------------------------------------------------------ *` |
|      - | 1464 | ` *  Rounding                                                           *` |
|      - | 1465 | ` * ------------------------------------------------------------------ */` |
|      - | 1466 | `/*` |
|      - | 1467 | ` * Round a to iPrec places under one of php's eight RoundingMode rules.` |
|      - | 1468 | ` *` |
|      - | 1469 | ` * The whole decision is made from the DROPPED digits: with nDrop of them, the` |
|      - | 1470 | ` * kept part T is the magnitude divided by 10^nDrop and the question is only` |
|      - | 1471 | ` * whether \|T\| gains one unit. The four HALF_* rules compare the dropped tail` |
|      - | 1472 | ` * against half of 10^nDrop, which over digits is "is the first dropped digit` |
|      - | 1473 | ` * above 5, below 5, or exactly 5 with nothing but zeros behind it"; the other` |
|      - | 1474 | ` * four never look at the tail's size at all, only at whether it is empty and` |
|      - | 1475 | ` * which way the sign points.` |
|      - | 1476 | ` *` |
|      - | 1477 | ` * The precision may be NEGATIVE (rounding to the left of the point), and then` |
|      - | 1478 | ` * the answer is the kept part with that many zeros put back -- which is why` |
|      - | 1479 | `` * `bcround('1', -3, AwayFromZero)` is '1000' and grows without bound as the`` |
|      - | 1480 | ` * precision falls, in php exactly as here.` |
|      - | 1481 | ` */` |
|    368 | 1482 | `static int BcNumRound(BcNum *pOut,const BcNum *a,sxi64 iPrec,int iMode)` |
|      1 | 1483 | `{` |
|      - | 1484 | `	BcNum sT, sOne, sSum;` |
|    369 | 1485 | `	sxi64 n = (sxi64)(a->nInt + a->nFrac);` |
|      - | 1486 | `	sxi64 nDrop, iFirst;` |
|    369 | 1487 | `	int dFirst = 0, bRest = 0, bInc = 0;` |
|    369 | 1488 | `	int rc = -1;` |
|      - | 1489 | ``	/* php's $precision reaches PHP_INT_MIN, and `nFrac - iPrec` would overflow`` |
|      - | 1490 | `	 * there. Every decision below reads the same answer from any nDrop past n,` |
|      - | 1491 | `	 * so the count is CLAMPED -- the true precision is still what the final` |
|      - | 1492 | `	 * left-shift uses, which is where a precision that far out really is felt. */` |
|    369 | 1493 | `	nDrop = (iPrec < -(n + 1)) ? (n + 1) : ((sxi64)a->nFrac - iPrec);` |
|    369 | 1494 | `	if( nDrop <= 0 ){` |
|      - | 1495 | `		/* Nothing below the target place: this is a PAD, not a rounding. */` |
|     17 | 1496 | `		if( BcNumCopy(pOut,a) \|\| BcNumSetScale(pOut,(sxu32)iPrec) ){` |
|    ! 0 | 1497 | `			return -1;` |
|      - | 1498 | `		}` |
|     17 | 1499 | `		return 0;` |
|      - | 1500 | `	}` |
|    353 | 1501 | `	BcNumInit(&sT,a->pAlloc);` |
|    353 | 1502 | `	BcNumInit(&sOne,a->pAlloc);` |
|    353 | 1503 | `	BcNumInit(&sSum,a->pAlloc);` |
|    353 | 1504 | `	if( BcNumShiftRight(&sT,a,nDrop >= n ? (sxu32)n : (sxu32)nDrop) ){` |
|    ! 0 | 1505 | `		goto out;` |
|      - | 1506 | `	}` |
|      - | 1507 | `	/* The first dropped digit, and whether anything nonzero follows it. Places` |
|      - | 1508 | `	 * above the number's own digits are zeros, so an index off the left end is a` |
|      - | 1509 | `	 * zero digit with the WHOLE magnitude behind it. */` |
|    353 | 1510 | `	iFirst = n - nDrop;` |
|    353 | 1511 | `	if( iFirst >= 0 && iFirst < n ){` |
|    301 | 1512 | `		dFirst = a->aDig[iFirst];` |
|    150 | 1513 | `	}` |
|      - | 1514 | `	{` |
|      - | 1515 | `		sxi64 j;` |
|    451 | 1516 | `		for( j = (iFirst < 0 ? 0 : iFirst + 1) ; j < n ; ++j ){` |
|    217 | 1517 | `			if( a->aDig[j] != 0 ){` |
|    119 | 1518 | `				bRest = 1;` |
|    119 | 1519 | `				break;` |
|      - | 1520 | `			}` |
|     50 | 1521 | `		}` |
|      - | 1522 | `	}` |
|    353 | 1523 | `	switch( iMode ){` |
|     15 | 1524 | `		case PH7_ROUND_TOWARD_ZERO:` |
|     31 | 1525 | `			bInc = 0;` |
|     31 | 1526 | `			break;` |
|     18 | 1527 | `		case PH7_ROUND_AWAY_FROM_ZERO:` |
|     37 | 1528 | `			bInc = (dFirst != 0 \|\| bRest);` |
|     37 | 1529 | `			break;` |
|     27 | 1530 | `		case PH7_ROUND_CEILING:      /* RoundingMode::PositiveInfinity */` |
|     55 | 1531 | `			bInc = (dFirst != 0 \|\| bRest) && !a->bNeg;` |
|     55 | 1532 | `			break;` |
|     27 | 1533 | `		case PH7_ROUND_FLOOR:        /* RoundingMode::NegativeInfinity */` |
|     55 | 1534 | `			bInc = (dFirst != 0 \|\| bRest) && a->bNeg;` |
|     55 | 1535 | `			break;` |
|     89 | 1536 | `		default: {` |
|      - | 1537 | `			/* The HALF_* family: above half, below half, or exactly half. */` |
|    179 | 1538 | `			if( dFirst > 5 \|\| (dFirst == 5 && bRest) ){` |
|     15 | 1539 | `				bInc = 1;` |
|    172 | 1540 | `			}else if( dFirst < 5 ){` |
|     79 | 1541 | `				bInc = 0;` |
|     40 | 1542 | `			}else{` |
|      - | 1543 | `				/* Exactly half. The last KEPT digit decides for the two parity` |
|      - | 1544 | `				 * rules; the other two decide from the direction alone. */` |
|     87 | 1545 | `				int dLast = sT.aDig[sT.nInt + sT.nFrac - 1];` |
|     87 | 1546 | `				switch( iMode ){` |
|     31 | 1547 | `					case PH7_ROUND_HALF_UP:   bInc = 1; break;` |
|     19 | 1548 | `					case PH7_ROUND_HALF_DOWN: bInc = 0; break;` |
|     21 | 1549 | `					case PH7_ROUND_HALF_EVEN: bInc = (dLast & 1); break;` |
|     19 | 1550 | `					default:                  bInc = !(dLast & 1); break; /* HALF_ODD */` |
|      - | 1551 | `				}` |
|      - | 1552 | `			}` |
|    178 | 1553 | `			break;` |
|      - | 1554 | `		}` |
|      - | 1555 | `	}` |
|    353 | 1556 | `	if( bInc ){` |
|    149 | 1557 | `		if( BcNumSmall(&sOne,1) \|\| BcMagAdd(&sSum,&sT,&sOne) ){` |
|    ! 0 | 1558 | `			goto out;` |
|      - | 1559 | `		}` |
|    149 | 1560 | `		BcNumSwap(&sT,&sSum);` |
|     74 | 1561 | `	}` |
|    353 | 1562 | `	if( iPrec >= 0 ){` |
|    239 | 1563 | `		BcNumSwap(pOut,&sT);` |
|    239 | 1564 | `		if( BcNumSetPoint(pOut,(sxu32)iPrec) ){` |
|    ! 0 | 1565 | `			goto out;` |
|      1 | 1566 | `		}` |
|    234 | 1567 | `	}else if( BcNumIsZero(&sT) ){` |
|      - | 1568 | `		/* Zero stays zero however far left the precision reaches -- and it is the` |
|      - | 1569 | `		 * only value that can, since a nonzero kept part means the precision is` |
|      - | 1570 | `		 * inside the number. */` |
|     53 | 1571 | `		if( BcNumSmall(pOut,0) ){` |
|    ! 0 | 1572 | `			goto out;` |
|      - | 1573 | `		}` |
|     27 | 1574 | `	}else{` |
|     63 | 1575 | `		if( -iPrec > (sxi64)BC_MAX_DIGITS \|\| BcNumShiftLeft(pOut,&sT,(sxu32)(-iPrec)) ){` |
|    ! 0 | 1576 | `			goto out;` |
|      - | 1577 | `		}` |
|      - | 1578 | `	}` |
|    353 | 1579 | `	pOut->bNeg = a->bNeg;` |
|    353 | 1580 | `	BcNumNormalize(pOut);` |
|    353 | 1581 | `	rc = 0;` |
|    176 | 1582 | `out:` |
|    353 | 1583 | `	BcNumRelease(&sT);` |
|    353 | 1584 | `	BcNumRelease(&sOne);` |
|    353 | 1585 | `	BcNumRelease(&sSum);` |
|    353 | 1586 | `	return rc;` |
|    185 | 1587 | `}` |
|      - | 1588 | `/*` |
|      - | 1589 | ` * string bcround(string $num, int $precision = 0, RoundingMode $mode = RoundingMode::HalfAwayFromZero)` |
|      - | 1590 | ` * string bcfloor(string $num)` |
|      - | 1591 | ` * string bcceil(string $num)` |
|      - | 1592 | ` *` |
|      - | 1593 | ` * The last two ARE bcround at precision 0 under the two infinity modes, which is` |
|      - | 1594 | ` * what php's own three answers show; only the argument list differs.` |
|      - | 1595 | ` */` |
|    362 | 1596 | `static int BcRoundOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode,` |
|      - | 1597 | `	const char *zFunc)` |
|      1 | 1598 | `{` |
|      - | 1599 | `	BcNum sA, sR;` |
|    363 | 1600 | `	sxi64 iPrec = 0;` |
|    363 | 1601 | `	int rc = PH7_OK;` |
|    363 | 1602 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|    363 | 1603 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|    363 | 1604 | `	if( nArg > 1 && iMode < 0 ){` |
|    285 | 1605 | `		iPrec = ph7_value_to_int64(apArg[1]);` |
|    285 | 1606 | `		if( iPrec > BC_MAX_SCALE ){` |
|      5 | 1607 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1608 | `				"bcround(): Argument #2 ($precision) must be between %qd and %d",` |
|      - | 1609 | `				(sxi64)(-SXI64_HIGH - 1),BC_MAX_SCALE);` |
|      5 | 1610 | `			goto done;` |
|      - | 1611 | `		}` |
|    140 | 1612 | `	}` |
|    359 | 1613 | `	if( iMode < 0 ){` |
|      - | 1614 | ``		/* The default is php's own `RoundingMode::HalfAwayFromZero`; anything the`` |
|      - | 1615 | `		 * caller passes is a CASE (the signature refuses every other type). */` |
|    303 | 1616 | `		iMode = PH7_ROUND_HALF_UP;` |
|    303 | 1617 | `		if( nArg > 2 ){` |
|    251 | 1618 | `			PH7_RoundingModeCase(apArg[2],&iMode);` |
|    125 | 1619 | `		}` |
|    151 | 1620 | `	}` |
|    359 | 1621 | `	if( !BcArgNum(pCtx,apArg[0],0,"num",zFunc,&sA) ){` |
|      7 | 1622 | `		goto done;` |
|      - | 1623 | `	}` |
|    353 | 1624 | `	if( BcNumRound(&sR,&sA,iPrec,iMode) ){` |
|    ! 0 | 1625 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1626 | `		goto done;` |
|      - | 1627 | `	}` |
|    353 | 1628 | `	rc = BcResultNum(pCtx,&sR);` |
|    181 | 1629 | `done:` |
|    363 | 1630 | `	BcNumRelease(&sA);` |
|    363 | 1631 | `	BcNumRelease(&sR);` |
|    363 | 1632 | `	return rc;` |
|      1 | 1633 | `}` |
|    306 | 1634 | `PH7_PRIVATE int PH7_builtin_bcround(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1635 | `{` |
|    307 | 1636 | `	return BcRoundOp(pCtx,nArg,apArg,-1,"bcround");` |
|      1 | 1637 | `}` |
|     28 | 1638 | `PH7_PRIVATE int PH7_builtin_bcfloor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1639 | `{` |
|     29 | 1640 | `	return BcRoundOp(pCtx,nArg,apArg,PH7_ROUND_FLOOR,"bcfloor");` |
|      1 | 1641 | `}` |
|     28 | 1642 | `PH7_PRIVATE int PH7_builtin_bcceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1643 | `{` |
|     29 | 1644 | `	return BcRoundOp(pCtx,nArg,apArg,PH7_ROUND_CEILING,"bcceil");` |
|      1 | 1645 | `}` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * int bcscale(?int $scale = null)` |
|      - | 1648 | ` *` |
|      - | 1649 | ` * Reads the directive with no argument (or a null one) and otherwise WRITES it,` |
|      - | 1650 | ` * answering what it held before -- one slot shared with ini_set('bcmath.scale').` |
|      - | 1651 | ` */` |
|     40 | 1652 | `PH7_PRIVATE int PH7_builtin_bcscale(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1653 | `{` |
|     41 | 1654 | `	sxu32 nOld = BcDefaultScale(pCtx->pVm);` |
|     41 | 1655 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     21 | 1656 | `		sxi64 iScale = ph7_value_to_int64(apArg[0]);` |
|      - | 1657 | `		char zBuf[32];` |
|      - | 1658 | `		int nBuf;` |
|     21 | 1659 | `		if( iScale < 0 \|\| iScale > BC_MAX_SCALE ){` |
|      5 | 1660 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1661 | `				"bcscale(): Argument #1 ($scale) must be between 0 and %d",` |
|      - | 1662 | `				BC_MAX_SCALE);` |
|      - | 1663 | `		}` |
|     17 | 1664 | `		nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%qd",iScale);` |
|     25 | 1665 | `		PH7_VmIniSet(pCtx->pVm,"bcmath.scale",sizeof("bcmath.scale")-1,` |
|      8 | 1666 | `			zBuf,(sxu32)nBuf,"bcscale()");` |
|      8 | 1667 | `	}` |
|     37 | 1668 | `	ph7_result_int64(pCtx,(sxi64)nOld);` |
|     37 | 1669 | `	return PH7_OK;` |
|     21 | 1670 | `}` |
|      - | 1671 | `/* ------------------------------------------------------------------ *` |
|      - | 1672 | ` *  BcMath\Number -- php 8.4's object face of the same arithmetic       *` |
|      - | 1673 | ` * ------------------------------------------------------------------ */` |
|      - | 1674 | `/*` |
|      - | 1675 | ` * The class is the SAME numbers with two differences that run through every` |
|      - | 1676 | ` * method, and both were measured rather than assumed:` |
|      - | 1677 | ` *` |
|      - | 1678 | `` *   - it carries its own SCALE and never reads `bcmath.scale`. Where a bc*`` |
|      - | 1679 | ` *     function pads or cuts to the directive, a method with no $scale computes` |
|      - | 1680 | ` *     one: max for add/sub/mod, the SUM for mul, 0 for a comparison's cut and` |
|      - | 1681 | ` *     for powmod, and for the three that do not terminate -- div, a negative` |
|      - | 1682 | ` *     pow, sqrt -- ten places past the RECEIVER's own scale, with the trailing` |
|      - | 1683 | ` *     zeros then trimmed but never below that receiver's scale. That last rule` |
|      - | 1684 | `` *     is why `Number('0.25')->sqrt()` is '0.50' and `Number('9')->sqrt()` is`` |
|      - | 1685 | ` *     '3'.` |
|      - | 1686 | `` *   - it answers php's do_operation, so `+ - * / % **`, unary minus and`` |
|      - | 1687 | `` *     `++`/`--` all work on it. An operand may be a Number, an integer, a bool`` |
|      - | 1688 | ` *     or a STRING in the bc grammar; a string that is not one is a ValueError` |
|      - | 1689 | ` *     naming the SIDE it came from, and everything else declines to the` |
|      - | 1690 | `` *     ordinary `Unsupported operand types`.`` |
|      - | 1691 | ` *` |
|      - | 1692 | `` * php reaches its `int` arm for a FLOAT operand through an implicit conversion`` |
|      - | 1693 | ` * it DEPRECATES when precision is lost. §10 refuses that: an integral float` |
|      - | 1694 | ` * converts (2.0 is 2), and every other one -- 1.5, NAN, INF, 1e20 -- is the` |
|      - | 1695 | ` * TypeError php itself raises for the three it cannot convert either.` |
|      - | 1696 | ` */` |
|      - | 1697 | `#define BC_NUMBER_CLASS "BcMath\\Number"` |
|      - | 1698 | `/* Ten places past the receiver's own scale: php's "compute enough and trim". */` |
|      - | 1699 | `#define BC_NUMBER_DIV_PAD 10` |
|      - | 1700 |  |
|      - | 1701 | `/* Trim trailing fraction zeros, but never below nMin places. */` |
|     54 | 1702 | `static void BcNumTrimScale(BcNum *p,sxu32 nMin)` |
|      1 | 1703 | `{` |
|    365 | 1704 | `	while( p->nFrac > nMin && p->aDig[p->nInt + p->nFrac - 1] == 0 ){` |
|    311 | 1705 | `		p->nFrac--;` |
|      1 | 1706 | `	}` |
|     55 | 1707 | `	BcNumNormalize(p);` |
|     55 | 1708 | `}` |
|      - | 1709 | `/* The BcNum a live Number instance holds. */` |
|    476 | 1710 | `static int BcNumberValue(ph7_class_instance *pObj,BcNum *pOut)` |
|      2 | 1711 | `{` |
|    478 | 1712 | `	const char *zVal = 0;` |
|    478 | 1713 | `	int nVal = 0;` |
|    478 | 1714 | `	PH7_NativeAttrStr(pObj,"value",&zVal,&nVal);` |
|    478 | 1715 | `	return BcNumParse(pOut,zVal,nVal) == 1 ? 0 : -1;` |
|      2 | 1716 | `}` |
|      - | 1717 | `/* A fresh Number carrying pVal. */` |
|    240 | 1718 | `static ph7_class_instance * BcNumberNew(ph7_vm *pVm,const BcNum *pVal)` |
|      2 | 1719 | `{` |
|    242 | 1720 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),BC_NUMBER_CLASS,` |
|      - | 1721 | `		sizeof(BC_NUMBER_CLASS)-1,FALSE,0);` |
|      - | 1722 | `	ph7_class_instance *pObj;` |
|      - | 1723 | `	SyBlob sTxt;` |
|    242 | 1724 | `	if( pClass == 0 ){` |
|    ! 0 | 1725 | `		return 0;` |
|      - | 1726 | `	}` |
|    242 | 1727 | `	pObj = PH7_NewClassInstance(&(*pVm),pClass);` |
|    242 | 1728 | `	if( pObj == 0 ){` |
|    ! 0 | 1729 | `		return 0;` |
|      - | 1730 | `	}` |
|    242 | 1731 | `	SyBlobInit(&sTxt,&pVm->sAllocator);` |
|    242 | 1732 | `	if( BcNumToBlob(pVal,&sTxt) ){` |
|    ! 0 | 1733 | `		SyBlobRelease(&sTxt);` |
|    ! 0 | 1734 | `		PH7_ClassInstanceUnref(pObj);` |
|    ! 0 | 1735 | `		return 0;` |
|      - | 1736 | `	}` |
|    362 | 1737 | `	PH7_NativeSetAttrStr(&(*pVm),pObj,"value",` |
|    240 | 1738 | `		(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));` |
|    242 | 1739 | `	SyBlobRelease(&sTxt);` |
|    242 | 1740 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,"scale",(sxi64)pVal->nFrac);` |
|    242 | 1741 | `	return pObj;` |
|    122 | 1742 | `}` |
|      - | 1743 | `/*` |
|      - | 1744 | ` * Read one operand as a bc number, for a METHOD (zFunc set) or for an OPERATOR` |
|      - | 1745 | ` * (zFunc 0, bLeft saying which side it came from).` |
|      - | 1746 | ` *` |
|      - | 1747 | ` * Answers 1 on success. On 0 the caller has a refusal in *pzClass/zMsg; on -1 a` |
|      - | 1748 | ` * memory failure. A value the class has no conversion for at all leaves` |
|      - | 1749 | ` * *pzClass at 0, which the OPERATOR path reads as "decline" -- the ordinary` |
|      - | 1750 | `` * numeric contract then words `Unsupported operand types` for it.`` |
|      - | 1751 | ` */` |
|    626 | 1752 | `static int BcNumberOperand(ph7_vm *pVm,ph7_value *pVal,BcNum *pOut,` |
|      - | 1753 | `	const char **pzClass,char *zMsg,int nMsg,` |
|      - | 1754 | `	const char *zFunc,const char *zTypeText,int iPos,const char *zParam,int bLeft)` |
|      2 | 1755 | `{` |
|      - | 1756 | `	char zGiven[64];` |
|    313 | 1757 | `	SXUNUSED(pVm);` |
|    628 | 1758 | `	*pzClass = 0;` |
|    628 | 1759 | `	if( (pVal->iFlags & MEMOBJ_OBJ) != 0 && pVal->x.pOther ){` |
|    210 | 1760 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|    208 | 1761 | `		if( pInst->pClass` |
|    208 | 1762 | `		 && pInst->pClass->sName.nByte == sizeof(BC_NUMBER_CLASS)-1` |
|    210 | 1763 | `		 && SyMemcmp(pInst->pClass->sName.zString,BC_NUMBER_CLASS,` |
|    104 | 1764 | `			sizeof(BC_NUMBER_CLASS)-1) == 0 ){` |
|    210 | 1765 | `			return BcNumberValue(pInst,pOut) == 0 ? 1 : -1;` |
|    ! 0 | 1766 | `		}` |
|    420 | 1767 | `	}else if( (pVal->iFlags & MEMOBJ_REAL) != 0 ){` |
|      - | 1768 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 1769 | ``		/* php's `int` arm, reached by an implicit conversion it deprecates when`` |
|      - | 1770 | `		 * anything is lost. An integral float in range converts; §10 refuses the` |
|      - | 1771 | `		 * rest, which is also what php does with NAN, INF and 1e20. */` |
|     43 | 1772 | `		double d = (double)pVal->rVal;` |
|     43 | 1773 | `		if( PH7_RealFitsInt64(d) && d == (double)(sxi64)d ){` |
|     15 | 1774 | `			return BcNumFromInt64(pOut,(sxi64)d) == 0 ? 1 : -1;` |
|      - | 1775 | `		}` |
|      - | 1776 | `#endif` |
|    391 | 1777 | `	}else if( (pVal->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL)) != 0` |
|    237 | 1778 | `	       && (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|     95 | 1779 | `		return BcNumFromInt64(pOut,pVal->x.iVal) == 0 ? 1 : -1;` |
|    284 | 1780 | `	}else if( (pVal->iFlags & MEMOBJ_STRING) != 0 ){` |
|      - | 1781 | `		const char *zStr;` |
|    276 | 1782 | `		int nStr = 0;` |
|      - | 1783 | `		int rc;` |
|    276 | 1784 | `		zStr = ph7_value_to_string(pVal,&nStr);` |
|    276 | 1785 | `		rc = BcNumParse(pOut,zStr,nStr);` |
|    276 | 1786 | `		if( rc == 1 ){` |
|    266 | 1787 | `			return 1;` |
|      - | 1788 | `		}` |
|     11 | 1789 | `		if( rc < 0 ){` |
|    ! 0 | 1790 | `			return -1;` |
|      - | 1791 | `		}` |
|     11 | 1792 | `		*pzClass = "ValueError";` |
|     11 | 1793 | `		if( zFunc ){` |
|     10 | 1794 | `			SyBufferFormat(zMsg,(sxu32)nMsg,"%s(): Argument #%d ($%s) is not well-formed",` |
|      3 | 1795 | `				zFunc,iPos,zParam);` |
|      4 | 1796 | `		}else{` |
|      7 | 1797 | `			SyBufferFormat(zMsg,(sxu32)nMsg,` |
|      - | 1798 | `				"%s string operand cannot be converted to " BC_NUMBER_CLASS,` |
|      2 | 1799 | `				bLeft ? "Left" : "Right");` |
|      - | 1800 | `		}` |
|     11 | 1801 | `		return 0;` |
|      - | 1802 | `	}` |
|     38 | 1803 | `	if( zFunc ){` |
|      - | 1804 | `		/* php words this one by hand rather than from the declared type: the stub` |
|      - | 1805 | ``		 * says `BcMath\Number\|string\|int` and the refusal says "int, string, or",`` |
|      - | 1806 | ``	 * while the CONSTRUCTOR's `string\|int` is worded the ordinary way. */`` |
|     12 | 1807 | `		*pzClass = "TypeError";` |
|     17 | 1808 | `		SyBufferFormat(zMsg,(sxu32)nMsg,` |
|      - | 1809 | `			"%s(): Argument #%d ($%s) must be of type %s, %s given",` |
|      5 | 1810 | `			zFunc,iPos,zParam,zTypeText,` |
|      5 | 1811 | `			VmValueGivenName(pVal,zGiven,sizeof(zGiven)));` |
|      5 | 1812 | `	}` |
|     38 | 1813 | `	return 0;` |
|    315 | 1814 | `}` |
|      - | 1815 | `/* The receiver of a BcMath\Number method. */` |
|    752 | 1816 | `static ph7_class_instance * BcNumberThis(ph7_context *pCtx)` |
|      2 | 1817 | `{` |
|    754 | 1818 | `	return PH7_ContextThis(pCtx);` |
|      2 | 1819 | `}` |
|      - | 1820 | ``/* Resolve a method's `?int $scale` argument: absent or null means "compute one". */`` |
|    208 | 1821 | `static int BcNumberArgScale(ph7_context *pCtx,int nArg,ph7_value **apArg,int iPos,` |
|      - | 1822 | `	const char *zFunc,sxu32 *pScale,int *pbAuto)` |
|      2 | 1823 | `{` |
|    210 | 1824 | `	*pbAuto = 1;` |
|    210 | 1825 | `	*pScale = 0;` |
|    210 | 1826 | `	if( nArg > iPos && (apArg[iPos]->iFlags & MEMOBJ_NULL) == 0 ){` |
|     37 | 1827 | `		sxi64 iScale = ph7_value_to_int64(apArg[iPos]);` |
|     37 | 1828 | `		if( iScale < 0 \|\| iScale > BC_MAX_SCALE ){` |
|      4 | 1829 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1830 | `				"%s(): Argument #%d ($scale) must be between 0 and %d",` |
|      1 | 1831 | `				zFunc,iPos + 1,BC_MAX_SCALE);` |
|      3 | 1832 | `			return 0;` |
|      - | 1833 | `		}` |
|     35 | 1834 | `		*pScale = (sxu32)iScale;` |
|     35 | 1835 | `		*pbAuto = 0;` |
|     17 | 1836 | `	}` |
|    208 | 1837 | `	return 1;` |
|    106 | 1838 | `}` |
|      - | 1839 | `/* Hand a computed number back as a fresh Number instance. */` |
|    174 | 1840 | `static int BcNumberResult(ph7_context *pCtx,const BcNum *pVal)` |
|      2 | 1841 | `{` |
|    176 | 1842 | `	ph7_class_instance *pObj = BcNumberNew(pCtx->pVm,pVal);` |
|    176 | 1843 | `	if( pObj == 0 ){` |
|    ! 0 | 1844 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1845 | `	}` |
|    176 | 1846 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    176 | 1847 | `	return PH7_OK;` |
|     89 | 1848 | `}` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * The one body behind every binary method AND behind the operator handler: the` |
|      - | 1851 | ` * two numbers are already parsed, so all that is left is which operation and` |
|      - | 1852 | ` * which scale.` |
|      - | 1853 | ` *` |
|      - | 1854 | ` * Answers 0 on success (pOut holds the answer), or -1 with *pzClass/zMsg set` |
|      - | 1855 | ` * for php's two arithmetic refusals (a zero divisor, a fractional or negative` |
|      - | 1856 | ` * exponent) and -2 for a memory failure.` |
|      - | 1857 | ` */` |
|      - | 1858 | `#define BC_NUM_ADD    0` |
|      - | 1859 | `#define BC_NUM_SUB    1` |
|      - | 1860 | `#define BC_NUM_MUL    2` |
|      - | 1861 | `#define BC_NUM_DIV    3` |
|      - | 1862 | `#define BC_NUM_MOD    4` |
|      - | 1863 | `#define BC_NUM_POW    5` |
|      - | 1864 | `#define BC_NUM_DIVMOD 6` |
|    196 | 1865 | `static int BcNumberCompute(ph7_vm *pVm,int iOp,const BcNum *a,const BcNum *b,` |
|      - | 1866 | `	sxu32 nScale,int bAuto,BcNum *pOut,BcNum *pQuot,` |
|      - | 1867 | `	const char **pzClass,char *zMsg,int nMsg)` |
|      2 | 1868 | `{` |
|    198 | 1869 | `	int rc = -2;` |
|    198 | 1870 | `	*pzClass = 0;` |
|    198 | 1871 | `	switch( iOp ){` |
|     32 | 1872 | `		case BC_NUM_ADD:` |
|      - | 1873 | `		case BC_NUM_SUB:` |
|     66 | 1874 | `			if( bAuto ){` |
|     60 | 1875 | `				nScale = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;` |
|     29 | 1876 | `			}` |
|     64 | 1877 | `			if( BcNumAddSigned(pOut,a,b,iOp == BC_NUM_SUB)` |
|     66 | 1878 | `			 \|\| BcNumSetScale(pOut,nScale) ){` |
|    ! 0 | 1879 | `				return -2;` |
|      - | 1880 | `			}` |
|     66 | 1881 | `			return 0;` |
|     12 | 1882 | `		case BC_NUM_MUL:` |
|     25 | 1883 | `			if( bAuto ){` |
|     25 | 1884 | `				nScale = a->nFrac + b->nFrac;` |
|     12 | 1885 | `			}` |
|     25 | 1886 | `			if( BcNumMul(pOut,a,b) \|\| BcNumSetScale(pOut,nScale) ){` |
|    ! 0 | 1887 | `				return -2;` |
|      - | 1888 | `			}` |
|     25 | 1889 | `			return 0;` |
|     17 | 1890 | `		case BC_NUM_DIV:` |
|     35 | 1891 | `			if( BcNumIsZero(b) ){` |
|      5 | 1892 | `				*pzClass = "DivisionByZeroError";` |
|      5 | 1893 | `				SyBufferFormat(zMsg,(sxu32)nMsg,"Division by zero");` |
|      5 | 1894 | `				return -1;` |
|      - | 1895 | `			}` |
|     31 | 1896 | `			if( bAuto ){` |
|      - | 1897 | `				/* Compute ten places past the RECEIVER's scale and trim back to it:` |
|      - | 1898 | ``				 * `Number('1.50')->div(1)` is '1.50' and `Number('1')->div(8)` is`` |
|      - | 1899 | `				 * '0.125'. */` |
|     24 | 1900 | `				if( a->nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD` |
|     25 | 1901 | `				 \|\| BcNumDivide(pOut,a,b,a->nFrac + BC_NUMBER_DIV_PAD) ){` |
|    ! 0 | 1902 | `					return -2;` |
|      - | 1903 | `				}` |
|     25 | 1904 | `				BcNumTrimScale(pOut,a->nFrac);` |
|     25 | 1905 | `				return 0;` |
|      - | 1906 | `			}` |
|      7 | 1907 | `			return BcNumDivide(pOut,a,b,nScale) ? -2 : 0;` |
|     19 | 1908 | `		case BC_NUM_MOD:` |
|      - | 1909 | `		case BC_NUM_DIVMOD:` |
|     39 | 1910 | `			if( BcNumIsZero(b) ){` |
|      7 | 1911 | `				*pzClass = "DivisionByZeroError";` |
|     10 | 1912 | `				SyBufferFormat(zMsg,(sxu32)nMsg,` |
|      3 | 1913 | `					iOp == BC_NUM_MOD ? "Modulo by zero" : "Division by zero");` |
|      7 | 1914 | `				return -1;` |
|      - | 1915 | `			}` |
|     33 | 1916 | `			if( bAuto ){` |
|     27 | 1917 | `				nScale = a->nFrac > b->nFrac ? a->nFrac : b->nFrac;` |
|     13 | 1918 | `			}` |
|     33 | 1919 | `			return BcNumDivMod(pQuot,pOut,a,b,nScale) ? -2 : 0;` |
|     18 | 1920 | `		case BC_NUM_POW: {` |
|     37 | 1921 | `			sxi64 iExp = 0;` |
|      - | 1922 | `			sxu64 uExp;` |
|      - | 1923 | `			BcNum sP, sOne;` |
|     37 | 1924 | `			if( BcNumHasFraction(b) ){` |
|      5 | 1925 | `				*pzClass = "ValueError";` |
|      5 | 1926 | `				SyBufferFormat(zMsg,(sxu32)nMsg,"exponent cannot have a fractional part");` |
|      5 | 1927 | `				return -1;` |
|      - | 1928 | `			}` |
|     33 | 1929 | `			if( !BcNumToInt64(b,&iExp) ){` |
|    ! 0 | 1930 | `				*pzClass = "ValueError";` |
|    ! 0 | 1931 | `				SyBufferFormat(zMsg,(sxu32)nMsg,"exponent is too large");` |
|    ! 0 | 1932 | `				return -1;` |
|      - | 1933 | `			}` |
|     33 | 1934 | `			if( iExp < 0 && BcNumIsZero(a) ){` |
|      3 | 1935 | `				*pzClass = "DivisionByZeroError";` |
|      3 | 1936 | `				SyBufferFormat(zMsg,(sxu32)nMsg,"Negative power of zero");` |
|      3 | 1937 | `				return -1;` |
|      - | 1938 | `			}` |
|      - | 1939 | `			/* -PHP_INT_MIN has no positive counterpart: negate in UNSIGNED. */` |
|     31 | 1940 | `			uExp = iExp < 0 ? ((sxu64)0 - (sxu64)iExp) : (sxu64)iExp;` |
|     31 | 1941 | `			BcNumInit(&sP,pVm ? &pVm->sAllocator : a->pAlloc);` |
|     31 | 1942 | `			BcNumInit(&sOne,sP.pAlloc);` |
|     31 | 1943 | `			if( BcNumPowInt(&sP,a,uExp) ){` |
|    ! 0 | 1944 | `				goto pow_out;` |
|      - | 1945 | `			}` |
|     31 | 1946 | `			if( iExp >= 0 ){` |
|     21 | 1947 | `				if( !bAuto && BcNumSetScale(&sP,nScale) ){` |
|    ! 0 | 1948 | `					goto pow_out;` |
|      - | 1949 | `				}` |
|     21 | 1950 | `				BcNumSwap(pOut,&sP);` |
|     21 | 1951 | `				rc = 0;` |
|     21 | 1952 | `				goto pow_out;` |
|      - | 1953 | `			}` |
|      - | 1954 | `			/* A negative exponent is 1 divided by the exact power, and the same` |
|      - | 1955 | `			 * "ten past the receiver, then trim" rule division uses. */` |
|     11 | 1956 | `			if( BcNumSmall(&sOne,1) ){` |
|    ! 0 | 1957 | `				goto pow_out;` |
|      - | 1958 | `			}` |
|     11 | 1959 | `			if( bAuto ){` |
|     10 | 1960 | `				if( a->nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD` |
|     11 | 1961 | `				 \|\| BcNumDivide(pOut,&sOne,&sP,a->nFrac + BC_NUMBER_DIV_PAD) ){` |
|    ! 0 | 1962 | `					goto pow_out;` |
|      - | 1963 | `				}` |
|     11 | 1964 | `				BcNumTrimScale(pOut,a->nFrac);` |
|      5 | 1965 | `			}else if( BcNumDivide(pOut,&sOne,&sP,nScale) ){` |
|    ! 0 | 1966 | `				goto pow_out;` |
|      - | 1967 | `			}` |
|     11 | 1968 | `			rc = 0;` |
|     15 | 1969 | `pow_out:` |
|     31 | 1970 | `			BcNumRelease(&sP);` |
|     31 | 1971 | `			BcNumRelease(&sOne);` |
|     31 | 1972 | `			return rc;` |
|      - | 1973 | `		}` |
|    ! 0 | 1974 | `		default:` |
|    ! 0 | 1975 | `			break;` |
|      - | 1976 | `	}` |
|    ! 0 | 1977 | `	return -2;` |
|    100 | 1978 | `}` |
|      - | 1979 | `/*` |
|      - | 1980 | `` * php's do_operation for BcMath\Number: `+ - * / % **`, either side.`` |
|      - | 1981 | ` */` |
|     68 | 1982 | `static void BcNumberArith(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeArithCtx *pCtx)` |
|      2 | 1983 | `{` |
|      - | 1984 | `	BcNum sA, sB, sR, sQ;` |
|     70 | 1985 | `	const char *zClass = 0;` |
|      - | 1986 | `	int iOp;` |
|      - | 1987 | `	int rc;` |
|     34 | 1988 | `	SXUNUSED(pThis);` |
|     70 | 1989 | `	switch( pCtx->zOp[0] ){` |
|     34 | 1990 | `		case '+': iOp = BC_NUM_ADD; break;` |
|     10 | 1991 | `		case '-': iOp = BC_NUM_SUB; break;` |
|      7 | 1992 | `		case '/': iOp = BC_NUM_DIV; break;` |
|      5 | 1993 | `		case '%': iOp = BC_NUM_MOD; break;` |
|     20 | 1994 | `		case '*': iOp = pCtx->zOp[1] == '*' ? BC_NUM_POW : BC_NUM_MUL; break;` |
|    ! 0 | 1995 | `		default:  return;   /* not an operator this class answers */` |
|      - | 1996 | `	}` |
|     70 | 1997 | `	BcNumInit(&sA,&pVm->sAllocator);` |
|     70 | 1998 | `	BcNumInit(&sB,&pVm->sAllocator);` |
|     70 | 1999 | `	BcNumInit(&sR,&pVm->sAllocator);` |
|     70 | 2000 | `	BcNumInit(&sQ,&pVm->sAllocator);` |
|    104 | 2001 | `	rc = BcNumberOperand(pVm,pCtx->pLeft,&sA,&zClass,` |
|     68 | 2002 | `		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg),0,0,0,0,1);` |
|     70 | 2003 | `	if( rc != 1 ){` |
|      3 | 2004 | `		goto done;` |
|      - | 2005 | `	}` |
|    101 | 2006 | `	rc = BcNumberOperand(pVm,pCtx->pRight,&sB,&zClass,` |
|     66 | 2007 | `		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg),0,0,0,0,0);` |
|     68 | 2008 | `	if( rc != 1 ){` |
|     20 | 2009 | `		goto done;` |
|      - | 2010 | `	}` |
|     74 | 2011 | `	rc = BcNumberCompute(pVm,iOp,&sA,&sB,0,1,&sR,&sQ,&zClass,` |
|     48 | 2012 | `		pCtx->zThrowMsg,(int)sizeof(pCtx->zThrowMsg));` |
|     71 | 2013 | `	if( rc == 0 ){` |
|     44 | 2014 | `		ph7_class_instance *pObj = BcNumberNew(pVm,&sR);` |
|     44 | 2015 | `		if( pObj ){` |
|     44 | 2016 | `			pCtx->pResult->x.pOther = pObj;` |
|     44 | 2017 | `			MemObjSetType(pCtx->pResult,MEMOBJ_OBJ);` |
|     44 | 2018 | `			pCtx->bHandled = 1;` |
|     21 | 2019 | `		}` |
|     21 | 2020 | `	}` |
|    ! 0 | 2021 | `done:` |
|     70 | 2022 | `	if( zClass ){` |
|     11 | 2023 | `		pCtx->zThrowClass = zClass;` |
|      5 | 2024 | `	}` |
|     70 | 2025 | `	BcNumRelease(&sA);` |
|     70 | 2026 | `	BcNumRelease(&sB);` |
|     70 | 2027 | `	BcNumRelease(&sR);` |
|     70 | 2028 | `	BcNumRelease(&sQ);` |
|     36 | 2029 | `}` |
|      - | 2030 | `/*` |
|      - | 2031 | ` * php's cast_object for _IS_BOOL: a Number is truthy unless it is ZERO, which is` |
|      - | 2032 | ` * the one place in the language where an object is not automatically true.` |
|      - | 2033 | ` */` |
|      4 | 2034 | `static int BcNumberBool(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 | 2035 | `{` |
|      - | 2036 | `	BcNum sA;` |
|      5 | 2037 | `	int bTruthy = 1;` |
|      5 | 2038 | `	BcNumInit(&sA,&pVm->sAllocator);` |
|      5 | 2039 | `	if( BcNumberValue(pThis,&sA) == 0 ){` |
|      5 | 2040 | `		bTruthy = !BcNumIsZero(&sA);` |
|      2 | 2041 | `	}` |
|      5 | 2042 | `	BcNumRelease(&sA);` |
|      5 | 2043 | `	return bTruthy;` |
|      1 | 2044 | `}` |
|      - | 2045 | `/*` |
|      - | 2046 | ` * php's compare handler: two Numbers, or a Number against an int or a numeric` |
|      - | 2047 | ` * STRING.` |
|      - | 2048 | ` *` |
|      - | 2049 | ` * NULL and BOOL are left alone on purpose -- php decides those pairs BEFORE it` |
|      - | 2050 | `` * asks a handler, by converting both sides to bool, so `$n == true` is true for`` |
|      - | 2051 | ` * every Number including zero. A FLOAT reaches php's int arm through the same` |
|      - | 2052 | ` * deprecated conversion the arithmetic uses; §10 refuses it, and the refusal's` |
|      - | 2053 | ` * shape in a comparison (which cannot throw) is php's own UNCOMPARABLE -- 1 from` |
|      - | 2054 | `` * either side, which leaves `==` false and every relational false.`` |
|      - | 2055 | ` */` |
|     42 | 2056 | `static void BcNumberCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)` |
|      2 | 2057 | `{` |
|      - | 2058 | `	BcNum sA, sB;` |
|      - | 2059 | `	/* A comparison cannot raise, so the operand reader's refusal is DISCARDED` |
|      - | 2060 | `	 * here: a partner it will not take is left to php's own rule, and a malformed` |
|      - | 2061 | `	 * string then compares AS a string -- which is php's answer for it too. */` |
|     44 | 2062 | `	const char *zClass = 0;` |
|      - | 2063 | `	char zMsg[64];` |
|     44 | 2064 | `	BcNumInit(&sA,&pVm->sAllocator);` |
|     44 | 2065 | `	BcNumInit(&sB,&pVm->sAllocator);` |
|     44 | 2066 | `	if( BcNumberValue(pThis,&sA) == 0 ){` |
|     44 | 2067 | `		ph7_value sOther, *pOther = pCtx->pOtherValue;` |
|     44 | 2068 | `		PH7_MemObjInit(&(*pVm),&sOther);` |
|     44 | 2069 | `		if( pOther == 0 && pCtx->pOther && pCtx->pOther->pClass ){` |
|      - | 2070 | `			/* The instance door hands the partner over as an INSTANCE; wrap it so` |
|      - | 2071 | `			 * one operand reader serves both. */` |
|     17 | 2072 | `			sOther.x.pOther = pCtx->pOther;` |
|     17 | 2073 | `			MemObjSetType(&sOther,MEMOBJ_OBJ);` |
|     17 | 2074 | `			pOther = &sOther;` |
|      8 | 2075 | `		}` |
|     44 | 2076 | `		if( pOther && (pOther->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) != 0 ){` |
|      5 | 2077 | `			pOther = 0;   /* php's own rule decides these */` |
|     41 | 2078 | `		}else if( pOther && (pOther->iFlags & MEMOBJ_REAL) != 0` |
|     29 | 2079 | `		       && (pOther->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      - | 2080 | `			/* A float: convertible only when nothing is lost (§10). Either way the` |
|      - | 2081 | `			 * pair is ANSWERED here, so no cast-the-object rule runs behind it. */` |
|     17 | 2082 | `			pCtx->bAnswered = 1;` |
|     16 | 2083 | `			if( BcNumberOperand(pVm,pOther,&sB,&zClass,zMsg,(int)sizeof(zMsg),` |
|      9 | 2084 | `				0,0,0,0,0) == 1 ){` |
|      7 | 2085 | `				pCtx->iResult = BcNumCmp(&sA,&sB);` |
|      7 | 2086 | `				if( pCtx->bReversed ){` |
|    ! 0 | 2087 | `					pCtx->iResult = -pCtx->iResult;` |
|    ! 0 | 2088 | `				}` |
|      3 | 2089 | `			}` |
|     17 | 2090 | `			pOther = 0;` |
|      8 | 2091 | `		}` |
|     42 | 2092 | `		if( pOther` |
|     34 | 2093 | `		 && BcNumberOperand(pVm,pOther,&sB,&zClass,zMsg,(int)sizeof(zMsg),0,0,0,0,0) == 1 ){` |
|     23 | 2094 | `			pCtx->iResult = BcNumCmp(&sA,&sB);` |
|     23 | 2095 | `			if( pCtx->bReversed ){` |
|    ! 0 | 2096 | `				pCtx->iResult = -pCtx->iResult;` |
|    ! 0 | 2097 | `			}` |
|     23 | 2098 | `			pCtx->bAnswered = 1;` |
|     11 | 2099 | `		}` |
|      - | 2100 | `		/* The wrapper never OWNED the instance: drop the pointer before release. */` |
|     44 | 2101 | `		MemObjSetType(&sOther,MEMOBJ_NULL);` |
|     44 | 2102 | `		sOther.x.pOther = 0;` |
|     44 | 2103 | `		PH7_MemObjRelease(&sOther);` |
|     21 | 2104 | `	}` |
|     44 | 2105 | `	BcNumRelease(&sA);` |
|     44 | 2106 | `	BcNumRelease(&sB);` |
|     44 | 2107 | `}` |
|      - | 2108 | `/*` |
|      - | 2109 | ` * BcMath\Number::__construct(string\|int $num)` |
|      - | 2110 | ` *` |
|      - | 2111 | `` * The type screen is hand-rolled (the row carries `~`) for one reason: php`` |
|      - | 2112 | `` * reaches the `int` arm for a FLOAT through the conversion §10 refuses, and a`` |
|      - | 2113 | `` * declared `string\|int` would quietly take the string arm instead --`` |
|      - | 2114 | `` * `new Number(1.5)` would be '1.5' where php answers '1'.`` |
|      - | 2115 | ` */` |
|    264 | 2116 | `static int vm_builtin_BcNumber_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2117 | `{` |
|    266 | 2118 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2119 | `	BcNum sVal;` |
|    266 | 2120 | `	const char *zClass = 0;` |
|      - | 2121 | `	char zMsg[192];` |
|      - | 2122 | `	int rc;` |
|    266 | 2123 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2124 | `		return PH7_OK;` |
|      - | 2125 | `	}` |
|    266 | 2126 | `	BcNumInit(&sVal,&pCtx->pVm->sAllocator);` |
|    266 | 2127 | `	rc = BcNumberOperand(pCtx->pVm,apArg[0],&sVal,&zClass,zMsg,(int)sizeof(zMsg),` |
|      - | 2128 | `		BC_NUMBER_CLASS "::__construct","string\|int",1,"num",0);` |
|    266 | 2129 | `	if( rc != 1 ){` |
|     10 | 2130 | `		BcNumRelease(&sVal);` |
|     10 | 2131 | `		if( rc < 0 ){` |
|    ! 0 | 2132 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2133 | `		}` |
|     10 | 2134 | `		return PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|      - | 2135 | `	}` |
|      - | 2136 | `	{` |
|      - | 2137 | `		SyBlob sTxt;` |
|    258 | 2138 | `		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);` |
|    258 | 2139 | `		if( BcNumToBlob(&sVal,&sTxt) ){` |
|    ! 0 | 2140 | `			SyBlobRelease(&sTxt);` |
|    ! 0 | 2141 | `			BcNumRelease(&sVal);` |
|    ! 0 | 2142 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2143 | `		}` |
|    386 | 2144 | `		PH7_NativeSetAttrStr(pCtx->pVm,pThis,"value",` |
|    256 | 2145 | `			(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));` |
|    258 | 2146 | `		SyBlobRelease(&sTxt);` |
|      - | 2147 | `	}` |
|    258 | 2148 | `	PH7_NativeSetAttrInt(pCtx->pVm,pThis,"scale",(sxi64)sVal.nFrac);` |
|    258 | 2149 | `	BcNumRelease(&sVal);` |
|    258 | 2150 | `	return PH7_OK;` |
|    134 | 2151 | `}` |
|      - | 2152 | `/*` |
|      - | 2153 | ` * add/sub/mul/div/mod/divmod/pow: one body, php's per-method wording on top.` |
|      - | 2154 | ` * The refusals php words WITHOUT a method prefix (a zero divisor, a fractional` |
|      - | 2155 | ` * exponent) come straight from the shared compute; the ones it words WITH a` |
|      - | 2156 | ` * prefix are raised here.` |
|      - | 2157 | ` */` |
|    156 | 2158 | `static int BcNumberBinary(ph7_context *pCtx,int nArg,ph7_value **apArg,int iOp,` |
|      - | 2159 | `	const char *zMethod)` |
|      2 | 2160 | `{` |
|    158 | 2161 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2162 | `	BcNum sA, sB, sR, sQ;` |
|    158 | 2163 | `	const char *zClass = 0;` |
|      - | 2164 | `	char zMsg[192];` |
|      - | 2165 | `	char zFunc[64];` |
|    158 | 2166 | `	sxu32 nScale = 0;` |
|    158 | 2167 | `	int bAuto = 1;` |
|    158 | 2168 | `	int rc = PH7_OK;` |
|      - | 2169 | `	int cc;` |
|    158 | 2170 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2171 | `		return PH7_OK;` |
|      - | 2172 | `	}` |
|    158 | 2173 | `	SyBufferFormat(zFunc,sizeof(zFunc),"%s::%s",BC_NUMBER_CLASS,zMethod);` |
|    158 | 2174 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|    158 | 2175 | `	BcNumInit(&sB,&pCtx->pVm->sAllocator);` |
|    158 | 2176 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|    158 | 2177 | `	BcNumInit(&sQ,&pCtx->pVm->sAllocator);` |
|    158 | 2178 | `	if( !BcNumberArgScale(pCtx,nArg,apArg,1,zFunc,&nScale,&bAuto) ){` |
|      3 | 2179 | `		goto done;` |
|      - | 2180 | `	}` |
|    156 | 2181 | `	if( BcNumberValue(pThis,&sA) ){` |
|    ! 0 | 2182 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2183 | `		goto done;` |
|      - | 2184 | `	}` |
|    233 | 2185 | `	cc = BcNumberOperand(pCtx->pVm,apArg[0],&sB,&zClass,zMsg,(int)sizeof(zMsg),` |
|     77 | 2186 | `		zFunc,"int, string, or " BC_NUMBER_CLASS,1,` |
|     77 | 2187 | `		iOp == BC_NUM_POW ? "exponent" : "num",0);` |
|    156 | 2188 | `	if( cc != 1 ){` |
|      8 | 2189 | `		if( cc < 0 ){` |
|    ! 0 | 2190 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2191 | `		}else{` |
|      8 | 2192 | `			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|      - | 2193 | `		}` |
|      8 | 2194 | `		goto done;` |
|      - | 2195 | `	}` |
|    224 | 2196 | `	cc = BcNumberCompute(pCtx->pVm,iOp,&sA,&sB,nScale,bAuto,&sR,&sQ,&zClass,` |
|     74 | 2197 | `		zMsg,(int)sizeof(zMsg));` |
|    150 | 2198 | `	if( cc == -2 ){` |
|    ! 0 | 2199 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2200 | `		goto done;` |
|      - | 2201 | `	}` |
|    150 | 2202 | `	if( cc == -1 ){` |
|     11 | 2203 | `		if( iOp == BC_NUM_POW && SyStrncmp(zMsg,"exponent",sizeof("exponent")-1) == 0 ){` |
|      - | 2204 | `			/* php's method wording repeats the parameter name after naming it. */` |
|      3 | 2205 | `			PH7_VmThrowException(pCtx,zClass,"%s(): Argument #1 ($exponent) %s",zFunc,zMsg);` |
|      2 | 2206 | `		}else{` |
|      9 | 2207 | `			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|      - | 2208 | `		}` |
|     11 | 2209 | `		goto done;` |
|      - | 2210 | `	}` |
|    140 | 2211 | `	if( iOp == BC_NUM_DIVMOD ){` |
|     13 | 2212 | `		ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     13 | 2213 | `		ph7_value *pCur = ph7_context_new_scalar(pCtx);` |
|     13 | 2214 | `		ph7_class_instance *pQObj = BcNumberNew(pCtx->pVm,&sQ);` |
|     13 | 2215 | `		ph7_class_instance *pRObj = BcNumberNew(pCtx->pVm,&sR);` |
|     13 | 2216 | `		if( pOut == 0 \|\| pCur == 0 \|\| pQObj == 0 \|\| pRObj == 0 ){` |
|    ! 0 | 2217 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2218 | `			goto done;` |
|      - | 2219 | `		}` |
|     13 | 2220 | `		pCur->x.pOther = pQObj;` |
|     13 | 2221 | `		MemObjSetType(pCur,MEMOBJ_OBJ);` |
|     13 | 2222 | `		ph7_array_add_elem(pOut,0,pCur);` |
|     13 | 2223 | `		pCur->x.pOther = pRObj;` |
|     13 | 2224 | `		ph7_array_add_elem(pOut,0,pCur);` |
|     13 | 2225 | `		MemObjSetType(pCur,MEMOBJ_NULL);` |
|     13 | 2226 | `		pCur->x.pOther = 0;` |
|     13 | 2227 | `		ph7_result_value(pCtx,pOut);` |
|     13 | 2228 | `		PH7_ClassInstanceUnref(pQObj);` |
|     13 | 2229 | `		PH7_ClassInstanceUnref(pRObj);` |
|     13 | 2230 | `		ph7_context_release_value(pCtx,pCur);` |
|     13 | 2231 | `		ph7_context_release_value(pCtx,pOut);` |
|     13 | 2232 | `		goto done;` |
|      - | 2233 | `	}` |
|    128 | 2234 | `	rc = BcNumberResult(pCtx,&sR);` |
|     78 | 2235 | `done:` |
|    158 | 2236 | `	BcNumRelease(&sA);` |
|    158 | 2237 | `	BcNumRelease(&sB);` |
|    158 | 2238 | `	BcNumRelease(&sR);` |
|    158 | 2239 | `	BcNumRelease(&sQ);` |
|    158 | 2240 | `	return rc;` |
|     80 | 2241 | `}` |
|     32 | 2242 | `static int vm_builtin_BcNumber_add(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     34 | 2243 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_ADD,"add"); }` |
|     16 | 2244 | `static int vm_builtin_BcNumber_sub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     17 | 2245 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_SUB,"sub"); }` |
|     16 | 2246 | `static int vm_builtin_BcNumber_mul(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     17 | 2247 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_MUL,"mul"); }` |
|     28 | 2248 | `static int vm_builtin_BcNumber_div(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     29 | 2249 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_DIV,"div"); }` |
|     20 | 2250 | `static int vm_builtin_BcNumber_mod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     21 | 2251 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_MOD,"mod"); }` |
|     14 | 2252 | `static int vm_builtin_BcNumber_divmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     15 | 2253 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_DIVMOD,"divmod"); }` |
|     30 | 2254 | `static int vm_builtin_BcNumber_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     31 | 2255 | `{ return BcNumberBinary(pCtx,nArg,apArg,BC_NUM_POW,"pow"); }` |
|      - | 2256 | `/*` |
|      - | 2257 | ` * BcMath\Number::powmod(BcMath\Number\|string\|int $exponent, BcMath\Number\|string\|int $modulus, ?int $scale = null)` |
|      - | 2258 | ` *` |
|      - | 2259 | ` * The scale is 0 unless one is asked for -- the receiver's own places never` |
|      - | 2260 | ` * reach the answer, because every operand is an integer by the time it runs.` |
|      - | 2261 | ` */` |
|     12 | 2262 | `static int vm_builtin_BcNumber_powmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2263 | `{` |
|      - | 2264 | `	static const char *azParam[] = { "exponent", "modulus" };` |
|     13 | 2265 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2266 | `	BcNum sA, aArg[2], sR;` |
|     13 | 2267 | `	const char *zClass = 0;` |
|      - | 2268 | `	char zMsg[192];` |
|      - | 2269 | `	char zFunc[64];` |
|     13 | 2270 | `	sxu32 nScale = 0;` |
|     13 | 2271 | `	int bAuto = 1;` |
|     13 | 2272 | `	int rc = PH7_OK;` |
|      - | 2273 | `	int i;` |
|     13 | 2274 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 2275 | `		return PH7_OK;` |
|      - | 2276 | `	}` |
|     13 | 2277 | `	SyBufferFormat(zFunc,sizeof(zFunc),"%s::powmod",BC_NUMBER_CLASS);` |
|     13 | 2278 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     13 | 2279 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     37 | 2280 | `	for( i = 0 ; i < 2 ; ++i ){` |
|     25 | 2281 | `		BcNumInit(&aArg[i],&pCtx->pVm->sAllocator);` |
|     13 | 2282 | `	}` |
|     13 | 2283 | `	if( !BcNumberArgScale(pCtx,nArg,apArg,2,zFunc,&nScale,&bAuto) ){` |
|    ! 0 | 2284 | `		goto done;` |
|      - | 2285 | `	}` |
|     13 | 2286 | `	if( BcNumberValue(pThis,&sA) ){` |
|    ! 0 | 2287 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2288 | `		goto done;` |
|      - | 2289 | `	}` |
|     37 | 2290 | `	for( i = 0 ; i < 2 ; ++i ){` |
|     37 | 2291 | `		int cc = BcNumberOperand(pCtx->pVm,apArg[i],&aArg[i],&zClass,zMsg,` |
|     12 | 2292 | `			(int)sizeof(zMsg),zFunc,"int, string, or " BC_NUMBER_CLASS,i + 1,azParam[i],0);` |
|     25 | 2293 | `		if( cc != 1 ){` |
|    ! 0 | 2294 | `			if( cc < 0 ){` |
|    ! 0 | 2295 | `				rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2296 | `			}else{` |
|    ! 0 | 2297 | `				PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|      - | 2298 | `			}` |
|    ! 0 | 2299 | `			goto done;` |
|      - | 2300 | `		}` |
|     13 | 2301 | `	}` |
|      - | 2302 | `	/* php names the RECEIVER without a prefix ("Base number ...") and the two` |
|      - | 2303 | `	 * arguments with one, in this order. */` |
|     13 | 2304 | `	if( BcNumHasFraction(&sA) ){` |
|      7 | 2305 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2306 | `			"Base number cannot have a fractional part");` |
|      7 | 2307 | `		goto done;` |
|      - | 2308 | `	}` |
|      7 | 2309 | `	if( BcNumHasFraction(&aArg[0]) ){` |
|    ! 0 | 2310 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|    ! 0 | 2311 | `			"%s(): Argument #1 ($exponent) cannot have a fractional part",zFunc);` |
|    ! 0 | 2312 | `		goto done;` |
|      - | 2313 | `	}` |
|      7 | 2314 | `	if( aArg[0].bNeg && !BcNumIsZero(&aArg[0]) ){` |
|    ! 0 | 2315 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|    ! 0 | 2316 | `			"%s(): Argument #1 ($exponent) must be greater than or equal to 0",zFunc);` |
|    ! 0 | 2317 | `		goto done;` |
|      - | 2318 | `	}` |
|      7 | 2319 | `	if( BcNumHasFraction(&aArg[1]) ){` |
|    ! 0 | 2320 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|    ! 0 | 2321 | `			"%s(): Argument #2 ($modulus) cannot have a fractional part",zFunc);` |
|    ! 0 | 2322 | `		goto done;` |
|      - | 2323 | `	}` |
|      7 | 2324 | `	if( BcNumIsZero(&aArg[1]) ){` |
|    ! 0 | 2325 | `		PH7_VmThrowException(pCtx,"DivisionByZeroError","Modulo by zero");` |
|    ! 0 | 2326 | `		goto done;` |
|      - | 2327 | `	}` |
|      7 | 2328 | `	if( BcNumPowMod(&sR,&sA,&aArg[0],&aArg[1]) \|\| BcNumSetScale(&sR,nScale) ){` |
|    ! 0 | 2329 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2330 | `		goto done;` |
|      - | 2331 | `	}` |
|      7 | 2332 | `	rc = BcNumberResult(pCtx,&sR);` |
|      6 | 2333 | `done:` |
|     13 | 2334 | `	BcNumRelease(&sA);` |
|     13 | 2335 | `	BcNumRelease(&sR);` |
|     37 | 2336 | `	for( i = 0 ; i < 2 ; ++i ){` |
|     25 | 2337 | `		BcNumRelease(&aArg[i]);` |
|     13 | 2338 | `	}` |
|     13 | 2339 | `	return rc;` |
|      7 | 2340 | `}` |
|      - | 2341 | `/*` |
|      - | 2342 | ` * BcMath\Number::sqrt(?int $scale = null)` |
|      - | 2343 | ` */` |
|     28 | 2344 | `static int vm_builtin_BcNumber_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2345 | `{` |
|     29 | 2346 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2347 | `	BcNum sA, sR;` |
|      - | 2348 | `	char zFunc[64];` |
|     29 | 2349 | `	sxu32 nScale = 0;` |
|     29 | 2350 | `	int bAuto = 1;` |
|     29 | 2351 | `	int rc = PH7_OK;` |
|     29 | 2352 | `	if( pThis == 0 ){` |
|    ! 0 | 2353 | `		return PH7_OK;` |
|      - | 2354 | `	}` |
|     29 | 2355 | `	SyBufferFormat(zFunc,sizeof(zFunc),"%s::sqrt",BC_NUMBER_CLASS);` |
|     29 | 2356 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     29 | 2357 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     29 | 2358 | `	if( !BcNumberArgScale(pCtx,nArg,apArg,0,zFunc,&nScale,&bAuto) ){` |
|    ! 0 | 2359 | `		goto done;` |
|      - | 2360 | `	}` |
|     29 | 2361 | `	if( BcNumberValue(pThis,&sA) ){` |
|    ! 0 | 2362 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2363 | `		goto done;` |
|      - | 2364 | `	}` |
|     29 | 2365 | `	if( sA.bNeg && !BcNumIsZero(&sA) ){` |
|      - | 2366 | `		/* php words this one from the RECEIVER, with no method prefix. */` |
|      3 | 2367 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2368 | `			"Base number must be greater than or equal to 0");` |
|      3 | 2369 | `		goto done;` |
|      - | 2370 | `	}` |
|     27 | 2371 | `	if( bAuto ){` |
|     20 | 2372 | `		if( sA.nFrac > BC_MAX_SCALE - BC_NUMBER_DIV_PAD` |
|     21 | 2373 | `		 \|\| BcNumSqrt(&sR,&sA,sA.nFrac + BC_NUMBER_DIV_PAD) ){` |
|    ! 0 | 2374 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2375 | `			goto done;` |
|      - | 2376 | `		}` |
|     21 | 2377 | `		BcNumTrimScale(&sR,sA.nFrac);` |
|     17 | 2378 | `	}else if( BcNumSqrt(&sR,&sA,nScale) ){` |
|    ! 0 | 2379 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2380 | `		goto done;` |
|      - | 2381 | `	}` |
|     27 | 2382 | `	rc = BcNumberResult(pCtx,&sR);` |
|     14 | 2383 | `done:` |
|     29 | 2384 | `	BcNumRelease(&sA);` |
|     29 | 2385 | `	BcNumRelease(&sR);` |
|     29 | 2386 | `	return rc;` |
|     15 | 2387 | `}` |
|      - | 2388 | `/*` |
|      - | 2389 | ` * BcMath\Number::floor() / ceil() / round(int $precision = 0, RoundingMode $mode = ...)` |
|      - | 2390 | ` */` |
|     18 | 2391 | `static int BcNumberRoundOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|      1 | 2392 | `{` |
|     19 | 2393 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2394 | `	BcNum sA, sR;` |
|     19 | 2395 | `	sxi64 iPrec = 0;` |
|     19 | 2396 | `	int rc = PH7_OK;` |
|     19 | 2397 | `	if( pThis == 0 ){` |
|    ! 0 | 2398 | `		return PH7_OK;` |
|      - | 2399 | `	}` |
|     19 | 2400 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     19 | 2401 | `	BcNumInit(&sR,&pCtx->pVm->sAllocator);` |
|     19 | 2402 | `	if( iMode < 0 ){` |
|     15 | 2403 | `		if( nArg > 0 ){` |
|     15 | 2404 | `			iPrec = ph7_value_to_int64(apArg[0]);` |
|     15 | 2405 | `			if( iPrec > BC_MAX_SCALE ){` |
|      3 | 2406 | `				PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2407 | `					"%s::round(): Argument #1 ($precision) must be between %qd and %d",` |
|      - | 2408 | `					BC_NUMBER_CLASS,(sxi64)(-SXI64_HIGH - 1),BC_MAX_SCALE);` |
|      3 | 2409 | `				goto done;` |
|      - | 2410 | `			}` |
|      6 | 2411 | `		}` |
|     13 | 2412 | `		iMode = PH7_ROUND_HALF_UP;` |
|     13 | 2413 | `		if( nArg > 1 ){` |
|      3 | 2414 | `			PH7_RoundingModeCase(apArg[1],&iMode);` |
|      1 | 2415 | `		}` |
|      6 | 2416 | `	}` |
|     17 | 2417 | `	if( BcNumberValue(pThis,&sA) \|\| BcNumRound(&sR,&sA,iPrec,iMode) ){` |
|    ! 0 | 2418 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2419 | `		goto done;` |
|      - | 2420 | `	}` |
|     17 | 2421 | `	rc = BcNumberResult(pCtx,&sR);` |
|      9 | 2422 | `done:` |
|     19 | 2423 | `	BcNumRelease(&sA);` |
|     19 | 2424 | `	BcNumRelease(&sR);` |
|     19 | 2425 | `	return rc;` |
|     10 | 2426 | `}` |
|     14 | 2427 | `static int vm_builtin_BcNumber_round(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     15 | 2428 | `{ return BcNumberRoundOp(pCtx,nArg,apArg,-1); }` |
|      2 | 2429 | `static int vm_builtin_BcNumber_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2430 | `{ return BcNumberRoundOp(pCtx,nArg,apArg,PH7_ROUND_FLOOR); }` |
|      2 | 2431 | `static int vm_builtin_BcNumber_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2432 | `{ return BcNumberRoundOp(pCtx,nArg,apArg,PH7_ROUND_CEILING); }` |
|      - | 2433 | `/*` |
|      - | 2434 | ` * BcMath\Number::compare(BcMath\Number\|string\|int $num, ?int $scale = null)` |
|      - | 2435 | ` *` |
|      - | 2436 | ` * With a $scale both sides are CUT to it first, exactly as bccomp() does; with` |
|      - | 2437 | ` * none the comparison is exact.` |
|      - | 2438 | ` */` |
|     12 | 2439 | `static int vm_builtin_BcNumber_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2440 | `{` |
|     13 | 2441 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2442 | `	BcNum sA, sB;` |
|     13 | 2443 | `	const char *zClass = 0;` |
|      - | 2444 | `	char zMsg[192];` |
|      - | 2445 | `	char zFunc[64];` |
|     13 | 2446 | `	sxu32 nScale = 0;` |
|     13 | 2447 | `	int bAuto = 1;` |
|     13 | 2448 | `	int rc = PH7_OK;` |
|      - | 2449 | `	int cc;` |
|     13 | 2450 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 2451 | `		return PH7_OK;` |
|      - | 2452 | `	}` |
|     13 | 2453 | `	SyBufferFormat(zFunc,sizeof(zFunc),"%s::compare",BC_NUMBER_CLASS);` |
|     13 | 2454 | `	BcNumInit(&sA,&pCtx->pVm->sAllocator);` |
|     13 | 2455 | `	BcNumInit(&sB,&pCtx->pVm->sAllocator);` |
|     13 | 2456 | `	if( !BcNumberArgScale(pCtx,nArg,apArg,1,zFunc,&nScale,&bAuto) ){` |
|    ! 0 | 2457 | `		goto done;` |
|      - | 2458 | `	}` |
|     13 | 2459 | `	if( BcNumberValue(pThis,&sA) ){` |
|    ! 0 | 2460 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2461 | `		goto done;` |
|      - | 2462 | `	}` |
|     19 | 2463 | `	cc = BcNumberOperand(pCtx->pVm,apArg[0],&sB,&zClass,zMsg,(int)sizeof(zMsg),` |
|      6 | 2464 | `		zFunc,"int, string, or " BC_NUMBER_CLASS,1,"num",0);` |
|     13 | 2465 | `	if( cc != 1 ){` |
|      3 | 2466 | `		if( cc < 0 ){` |
|    ! 0 | 2467 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2468 | `		}else{` |
|      3 | 2469 | `			PH7_VmThrowException(pCtx,zClass,"%s",zMsg);` |
|      - | 2470 | `		}` |
|      3 | 2471 | `		goto done;` |
|      - | 2472 | `	}` |
|     11 | 2473 | `	if( !bAuto && (BcNumSetScale(&sA,nScale) \|\| BcNumSetScale(&sB,nScale)) ){` |
|    ! 0 | 2474 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2475 | `		goto done;` |
|      - | 2476 | `	}` |
|     11 | 2477 | `	ph7_result_int(pCtx,BcNumCmp(&sA,&sB));` |
|      6 | 2478 | `done:` |
|     13 | 2479 | `	BcNumRelease(&sA);` |
|     13 | 2480 | `	BcNumRelease(&sB);` |
|     13 | 2481 | `	return rc;` |
|      7 | 2482 | `}` |
|      - | 2483 | `/*` |
|      - | 2484 | ` * BcMath\Number::__toString()` |
|      - | 2485 | ` */` |
|    254 | 2486 | `static int vm_builtin_BcNumber_toString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2487 | `{` |
|    256 | 2488 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|    256 | 2489 | `	const char *zVal = 0;` |
|    256 | 2490 | `	int nVal = 0;` |
|    127 | 2491 | `	SXUNUSED(nArg);` |
|    127 | 2492 | `	SXUNUSED(apArg);` |
|    256 | 2493 | `	if( pThis ){` |
|    256 | 2494 | `		PH7_NativeAttrStr(pThis,"value",&zVal,&nVal);` |
|    127 | 2495 | `	}` |
|    256 | 2496 | `	ph7_result_string(pCtx,zVal ? zVal : "0",zVal ? nVal : 1);` |
|    256 | 2497 | `	return PH7_OK;` |
|      2 | 2498 | `}` |
|      - | 2499 | `/*` |
|      - | 2500 | ` * BcMath\Number::__serialize() / __unserialize(array $data)` |
|      - | 2501 | ` *` |
|      - | 2502 | ` * Only the VALUE travels -- the scale follows from it, which is why php's` |
|      - | 2503 | `` * serialization is a one-key array and its `O:13:...` form carries one property.`` |
|      - | 2504 | ` */` |
|      6 | 2505 | `static int vm_builtin_BcNumber_serialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2506 | `{` |
|      7 | 2507 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2508 | `	ph7_value *pOut, *pCur;` |
|      7 | 2509 | `	const char *zVal = 0;` |
|      7 | 2510 | `	int nVal = 0;` |
|      3 | 2511 | `	SXUNUSED(nArg);` |
|      3 | 2512 | `	SXUNUSED(apArg);` |
|      7 | 2513 | `	pOut = ph7_context_new_array(pCtx);` |
|      7 | 2514 | `	pCur = ph7_context_new_scalar(pCtx);` |
|      7 | 2515 | `	if( pOut == 0 \|\| pCur == 0 ){` |
|    ! 0 | 2516 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2517 | `	}` |
|      7 | 2518 | `	if( pThis ){` |
|      7 | 2519 | `		PH7_NativeAttrStr(pThis,"value",&zVal,&nVal);` |
|      3 | 2520 | `	}` |
|      7 | 2521 | `	ph7_value_string(pCur,zVal ? zVal : "0",zVal ? nVal : 1);` |
|      7 | 2522 | `	ph7_array_add_strkey_elem(pOut,"value",pCur);` |
|      7 | 2523 | `	ph7_result_value(pCtx,pOut);` |
|      7 | 2524 | `	ph7_context_release_value(pCtx,pCur);` |
|      7 | 2525 | `	ph7_context_release_value(pCtx,pOut);` |
|      7 | 2526 | `	return PH7_OK;` |
|      4 | 2527 | `}` |
|      2 | 2528 | `static int vm_builtin_BcNumber_unserialize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2529 | `{` |
|      3 | 2530 | `	ph7_class_instance *pThis = BcNumberThis(pCtx);` |
|      - | 2531 | `	ph7_value *pVal;` |
|      - | 2532 | `	BcNum sVal;` |
|      3 | 2533 | `	int rc = PH7_OK;` |
|      3 | 2534 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 2535 | `		return PH7_VmThrowException(pCtx,"Exception","Invalid serialization data for "` |
|      - | 2536 | `			BC_NUMBER_CLASS " object");` |
|      - | 2537 | `	}` |
|      3 | 2538 | `	pVal = ph7_array_fetch(apArg[0],"value",sizeof("value")-1);` |
|      3 | 2539 | `	BcNumInit(&sVal,&pCtx->pVm->sAllocator);` |
|      2 | 2540 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0` |
|      3 | 2541 | `	 \|\| BcNumParse(&sVal,(const char *)SyBlobData(&pVal->sBlob),` |
|      3 | 2542 | `		(int)SyBlobLength(&pVal->sBlob)) != 1 ){` |
|    ! 0 | 2543 | `		BcNumRelease(&sVal);` |
|    ! 0 | 2544 | `		return PH7_VmThrowException(pCtx,"Exception","Invalid serialization data for "` |
|      - | 2545 | `			BC_NUMBER_CLASS " object");` |
|      - | 2546 | `	}` |
|      - | 2547 | `	{` |
|      - | 2548 | `		SyBlob sTxt;` |
|      3 | 2549 | `		SyBlobInit(&sTxt,&pCtx->pVm->sAllocator);` |
|      3 | 2550 | `		if( BcNumToBlob(&sVal,&sTxt) ){` |
|    ! 0 | 2551 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 2552 | `		}else{` |
|      4 | 2553 | `			PH7_NativeSetAttrStr(pCtx->pVm,pThis,"value",` |
|      2 | 2554 | `				(const char *)SyBlobData(&sTxt),(int)SyBlobLength(&sTxt));` |
|      3 | 2555 | `			PH7_NativeSetAttrInt(pCtx->pVm,pThis,"scale",(sxi64)sVal.nFrac);` |
|      - | 2556 | `		}` |
|      3 | 2557 | `		SyBlobRelease(&sTxt);` |
|      - | 2558 | `	}` |
|      3 | 2559 | `	BcNumRelease(&sVal);` |
|      3 | 2560 | `	return rc;` |
|      2 | 2561 | `}` |
|      - | 2562 | `/*` |
|      - | 2563 | ` * Declare BcMath\Number.` |
|      - | 2564 | ` */` |
|   5742 | 2565 | `PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm)` |
|      5 | 2566 | `{` |
|      - | 2567 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 2568 | `		{ "value", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|      - | 2569 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 2570 | `		{ "scale", PH7_MOD_PUBLIC\|PH7_MOD_PROT_SET\|PH7_MOD_READONLY,` |
|      - | 2571 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" }` |
|      - | 2572 | `	};` |
|      - | 2573 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 2574 | ``		/* `~` on the first parameter of every one of these: php's stub declares a`` |
|      - | 2575 | `		 * UNION and its refusal words a different one ("int, string, or"), and the` |
|      - | 2576 | `		 * float arm is the conversion §10 refuses -- both of which the generic` |
|      - | 2577 | `		 * screen cannot express, so each body raises its own. */` |
|      - | 2578 | `		{ "__construct", PH7_MOD_PUBLIC, "~string\|int $num", 0,` |
|      - | 2579 | `		  vm_builtin_BcNumber_construct },` |
|      - | 2580 | `		{ "add", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2581 | `		  "BcMath\\Number", vm_builtin_BcNumber_add },` |
|      - | 2582 | `		{ "sub", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2583 | `		  "BcMath\\Number", vm_builtin_BcNumber_sub },` |
|      - | 2584 | `		{ "mul", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2585 | `		  "BcMath\\Number", vm_builtin_BcNumber_mul },` |
|      - | 2586 | `		{ "div", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2587 | `		  "BcMath\\Number", vm_builtin_BcNumber_div },` |
|      - | 2588 | `		{ "mod", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2589 | `		  "BcMath\\Number", vm_builtin_BcNumber_mod },` |
|      - | 2590 | `		{ "divmod", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2591 | `		  "array", vm_builtin_BcNumber_divmod },` |
|      - | 2592 | `		{ "powmod", PH7_MOD_PUBLIC,` |
|      - | 2593 | `		  "~BcMath\\Number\|string\|int $exponent, ~BcMath\\Number\|string\|int $modulus, ?int $scale = NULL",` |
|      - | 2594 | `		  "BcMath\\Number", vm_builtin_BcNumber_powmod },` |
|      - | 2595 | `		{ "pow", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $exponent, ?int $scale = NULL",` |
|      - | 2596 | `		  "BcMath\\Number", vm_builtin_BcNumber_pow },` |
|      - | 2597 | `		{ "sqrt", PH7_MOD_PUBLIC, "?int $scale = NULL", "BcMath\\Number",` |
|      - | 2598 | `		  vm_builtin_BcNumber_sqrt },` |
|      - | 2599 | `		{ "floor", PH7_MOD_PUBLIC, "", "BcMath\\Number", vm_builtin_BcNumber_floor },` |
|      - | 2600 | `		{ "ceil", PH7_MOD_PUBLIC, "", "BcMath\\Number", vm_builtin_BcNumber_ceil },` |
|      - | 2601 | `		{ "round", PH7_MOD_PUBLIC, "int $precision = 0, RoundingMode $mode = ?",` |
|      - | 2602 | `		  "BcMath\\Number", vm_builtin_BcNumber_round },` |
|      - | 2603 | `		{ "compare", PH7_MOD_PUBLIC, "~BcMath\\Number\|string\|int $num, ?int $scale = NULL",` |
|      - | 2604 | `		  "int", vm_builtin_BcNumber_compare },` |
|      - | 2605 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_BcNumber_toString },` |
|      - | 2606 | `		{ "__serialize", PH7_MOD_PUBLIC, "", "array", vm_builtin_BcNumber_serialize },` |
|      - | 2607 | `		{ "__unserialize", PH7_MOD_PUBLIC, "array $data", "void",` |
|      - | 2608 | `		  vm_builtin_BcNumber_unserialize }` |
|      - | 2609 | `	};` |
|      - | 2610 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 2611 | `		BC_NUMBER_CLASS, 0, "Stringable",` |
|      - | 2612 | `		PH7_CLASS_FINAL\|PH7_CLASS_READONLY,` |
|      - | 2613 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|      - | 2614 | `		0, 0,` |
|      - | 2615 | `		aProp, SX_ARRAYSIZE(aProp),` |
|      - | 2616 | `		0, 0, 0` |
|      - | 2617 | `	};` |
|   5747 | 2618 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|   5747 | 2619 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 2620 | `		return rc;` |
|      - | 2621 | `	}` |
|      - | 2622 | `	/* php builds both properties out of its own struct rather than storing them,` |
|      - | 2623 | ``	 * which is what `virtual` reports and what keeps the object comparator off`` |
|      - | 2624 | `	 * them -- the compare handler below decides every pair. */` |
|   5747 | 2625 | `	PH7_NativeClassMarkVirtualProps(&(*pVm),BC_NUMBER_CLASS);` |
|   5747 | 2626 | `	PH7_NativeClassInstallCmpHook(&(*pVm),BC_NUMBER_CLASS,BcNumberCmp);` |
|   5747 | 2627 | `	PH7_NativeClassInstallBoolHook(&(*pVm),BC_NUMBER_CLASS,BcNumberBool);` |
|   5747 | 2628 | `	PH7_NativeClassInstallArithHook(&(*pVm),BC_NUMBER_CLASS,BcNumberArith);` |
|   5747 | 2629 | `	return SXRET_OK;` |
|   2877 | 2630 | `}` |
|      - | 2631 | `#else` |
|      - | 2632 | `/* The tiny build has no bc* functions, so it has no class for them either. */` |
|      - | 2633 | `PH7_PRIVATE sxi32 PH7_VmInstallBcMath(ph7_vm *pVm){ SXUNUSED(pVm); return SXRET_OK; }` |
|      - | 2634 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 2635 |  |
