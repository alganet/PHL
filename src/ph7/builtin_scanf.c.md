# src/ph7/builtin_scanf.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 633/696 lines (90.95%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#include <stdlib.h>  /* strtod (the %f/%e/%g conversion) */` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *    Formatted input: sscanf() and the scanner fscanf() shares with it.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` *` |
|    - |   13 | ` * php's scanf is not C's. It came from Tcl 8.3 by way of ext/standard/scanf.c` |
|    - |   14 | ` * and keeps that lineage's rules, several of which a re-derivation from` |
|    - |   15 | ` * scanf(3) gets wrong:` |
|    - |   16 | ` *` |
|    - |   17 | ` *   - the format is READ TWICE. The first pass validates it and counts the` |
|    - |   18 | ` *     substitutions, raising every ValueError this family has before a single` |
|    - |   19 | ` *     byte of input is looked at; the second pass does the scanning. So a` |
|    - |   20 | ` *     format whose LAST specifier is bad refuses the whole call, and a bad` |
|    - |   21 | ` *     one is a thrown error rather than a false return.` |
|    - |   22 | `` *   - `%c` is not C's single character: it is `%s` with the whitespace SKIP`` |
|    - |   23 | ``  *     turned off and a default width of one, so it stops at a space. `"  ab"` `` |
|    - |   24 | `` *     read with `%c` answers the EMPTY string, and counts as a conversion.`` |
|    - |   25 | `` *   - `%n$` (XPG3 positional) may not be mixed with plain `%`, a suppressed`` |
|    - |   26 | `` *     `%*d` counts as neither, and with no variables passed the highest index`` |
|    - |   27 | ` *     decides how long the answer array is.` |
|    - |   28 | ` *   - a conversion the input cannot satisfy stops the scan where it stands and` |
|    - |   29 | ` *     everything after it stays NULL; only hitting the end of the input with` |
|    - |   30 | ` *     NOTHING converted is the -1 / NULL "EOF" answer.` |
|    - |   31 | ` *   - the value is scanned into a 64-byte buffer, so a width past 63 is` |
|    - |   32 | ` *     clamped, and both the integer and the float scanners BACK UP over a` |
|    - |   33 | `` *     trailing `0x` / `e` / sign they turned out not to be able to use.`` |
|    - |   34 | ` *` |
|    - |   35 | ` * Both builtins take the string as a C string, php's own limitation: a NUL` |
|    - |   36 | ` * byte in either the subject or the format ends it there.` |
|    - |   37 | ` */` |
|    - |   38 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|    - |   39 | `/*` |
|    - |   40 | `` * php caps a positional `%n$` index at 255 when no variables were passed --`` |
|    - |   41 | ` * "in the interest of security and resource friendliness", since the index` |
|    - |   42 | ` * alone decides how large the answer array is.` |
|    - |   43 | ` */` |
|    - |   44 | `#define SCANF_MAX_ARGS 0xFF` |
|    - |   45 | `/* The scratch a number is accumulated into before it is converted. Its size is` |
|    - |   46 | ` * what bounds a field WIDTH for the numeric conversions. */` |
|    - |   47 | `#define SCANF_NUM_BUF   64` |
|    - |   48 | `/* Per-specifier flags, php's own set. */` |
|    - |   49 | `#define SCANF_NOSKIP    0x001  /* Do not skip leading whitespace (%c and %[) */` |
|    - |   50 | ``#define SCANF_SUPPRESS  0x002  /* `%*` -- scan the field, assign nothing */`` |
|    - |   51 | `#define SCANF_UNSIGNED  0x004  /* %u -- the value is read as unsigned */` |
|    - |   52 | `#define SCANF_SIGNOK    0x010  /* a +/- is still allowed here */` |
|    - |   53 | `#define SCANF_NODIGITS  0x020  /* no digit has been accepted yet */` |
|    - |   54 | `#define SCANF_NOZERO    0x040  /* no leading zero has been accepted yet */` |
|    - |   55 | ``#define SCANF_XOK       0x080  /* an `x` may follow (a 0 in base 16 / base 0) */`` |
|    - |   56 | `#define SCANF_PTOK      0x100  /* the decimal point is still allowed */` |
|    - |   57 | `#define SCANF_EXPOK     0x200  /* an exponent is still allowed */` |
|    - |   58 | `/*` |
|    - |   59 | `` * A `%[...]` character set: loose characters and ranges kept apart, exactly as`` |
|    - |   60 | ` * php builds them.` |
|    - |   61 | ` *` |
|    - |   62 | ` * The comparisons are made on a SIGNED char on purpose, because php's are: its` |
|    - |   63 | `` * CharSet holds `char` and compares with `<=`, so on the platforms this engine`` |
|    - |   64 | ` * targets a byte at or above 0x80 is NEGATIVE, and that decides both what a` |
|    - |   65 | ``  * range contains and whether php considers it written backwards. `%[\x01-\xff]` `` |
|    - |   66 | ` * is therefore the range 0xff..0x01 read back to front, and matches nothing an` |
|    - |   67 | ` * ASCII string can offer -- an answer only a differential finds, and one a` |
|    - |   68 | ` * rewrite over unsigned bytes would quietly "fix".` |
|    - |   69 | ` */` |
|    - |   70 | `typedef struct scanf_charset {` |
|    - |   71 | ``	int exclude;              /* `^` -- the set is what is NOT listed */`` |
|    - |   72 | `	int nchars;` |
|    - |   73 | `	signed char *chars;` |
|    - |   74 | `	int nranges;` |
|    - |   75 | `	struct scanf_range {` |
|    - |   76 | `		signed char start;` |
|    - |   77 | `		signed char end;` |
|    - |   78 | `	} *ranges;` |
|    - |   79 | `} scanf_charset;` |
|    - |   80 | `/*` |
|    - |   81 | ` * Where a converted value goes. Both callers hand the same sink in: with` |
|    - |   82 | ` * variables passed, each conversion writes through one of them and the call` |
|    - |   83 | ` * answers how many conversions happened; with none, the call answers an ARRAY` |
|    - |   84 | ` * pre-filled with as many NULLs as the format has substitutions, and a` |
|    - |   85 | ` * conversion overwrites its own slot.` |
|    - |   86 | ` */` |
|    - |   87 | `typedef struct scanf_sink {` |
|    - |   88 | `	ph7_context *pCtx;` |
|    - |   89 | `	ph7_value **apVar;   /* the by-reference variadic tail, or 0 */` |
|    - |   90 | `	int nVar;            /* how many of them */` |
|    - |   91 | `	ph7_value *pArray;   /* the answer array, when nVar == 0 */` |
|    - |   92 | `	ph7_value *pTmp;     /* scratch for one element/assignment */` |
|    - |   93 | `} scanf_sink;` |
|    - |   94 | ``/* Is this byte one of php's `isspace` set? Written out rather than asked of`` |
|    - |   95 | ` * libc, which answers by LC_CTYPE where php's scanner does not. */` |
|  724 |   96 | `static int ScanfIsSpace(int c)` |
|    2 |   97 | `{` |
|  726 |   98 | `	return c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\v' \|\| c == '\f' \|\| c == '\r';` |
|    2 |   99 | `}` |
| 1580 |  100 | `static int ScanfIsDigit(int c)` |
|    2 |  101 | `{` |
| 1582 |  102 | `	return c >= '0' && c <= '9';` |
|    2 |  103 | `}` |
|    - |  104 | `/*` |
|    - |  105 | ` * Read a decimal run, saturating rather than wrapping -- strtoul's answer for` |
|    - |  106 | ` * an over-wide run is its maximum, and php reads both the positional index and` |
|    - |  107 | ` * the field width through it.` |
|    - |  108 | ` */` |
|   96 |  109 | `static sxu64 ScanfReadUInt(const char *zIn,const char **pzEnd)` |
|    1 |  110 | `{` |
|   97 |  111 | `	sxu64 uVal = 0;` |
|   97 |  112 | `	int bOver = 0;` |
|  197 |  113 | `	while( ScanfIsDigit((unsigned char)zIn[0]) ){` |
|  101 |  114 | `		int d = zIn[0] - '0';` |
|  101 |  115 | `		if( uVal > (SXU64_HIGH - (sxu64)d) / 10 ){` |
|  ! 0 |  116 | `			bOver = 1;` |
|  ! 0 |  117 | `		}else{` |
|  101 |  118 | `			uVal = uVal * 10 + (sxu64)d;` |
|    - |  119 | `		}` |
|  101 |  120 | `		zIn++;` |
|    1 |  121 | `	}` |
|   97 |  122 | `	if( pzEnd ){` |
|   97 |  123 | `		*pzEnd = zIn;` |
|   48 |  124 | `	}` |
|   97 |  125 | `	return bOver ? SXU64_HIGH : uVal;` |
|    1 |  126 | `}` |
|    - |  127 | `/*` |
|    - |  128 | ` * strtoll()/strtoull() over the accumulated buffer, which the scanners have` |
|    - |  129 | `` * already restricted to a sign, an optional `0x` and digits of the base. The`` |
|    - |  130 | ` * conversion saturates the way the C library's does, which is what makes` |
|    - |  131 | `` * `sscanf("9999999999999999999999","%d")` answer PHP_INT_MAX rather than a`` |
|    - |  132 | `` * wrapped number; `%u` then reinterprets that saturated width as unsigned.`` |
|    - |  133 | ` */` |
|  112 |  134 | `static sxu64 ScanfStrToNum(const char *zBuf,int iBase,int bUnsigned)` |
|    2 |  135 | `{` |
|  114 |  136 | `	const char *z = zBuf;` |
|  114 |  137 | `	sxu64 uVal = 0;` |
|  114 |  138 | `	int bNeg = 0, bOver = 0;` |
|  114 |  139 | `	if( z[0] == '+' \|\| z[0] == '-' ){` |
|    9 |  140 | `		bNeg = (z[0] == '-');` |
|    9 |  141 | `		z++;` |
|    4 |  142 | `	}` |
|  114 |  143 | `	if( iBase == 16 && z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|    5 |  144 | `		z += 2;` |
|    2 |  145 | `	}` |
|  326 |  146 | `	for(;;){` |
|    - |  147 | `		int d;` |
|  384 |  148 | `		int c = (unsigned char)z[0];` |
|  384 |  149 | `		if( ScanfIsDigit(c) ){` |
|  266 |  150 | `			d = c - '0';` |
|  252 |  151 | `		}else if( c >= 'a' && c <= 'f' ){` |
|    5 |  152 | `			d = c - 'a' + 10;` |
|  118 |  153 | `		}else if( c >= 'A' && c <= 'F' ){` |
|    3 |  154 | `			d = c - 'A' + 10;` |
|    2 |  155 | `		}else{` |
|   58 |  156 | `			break;` |
|    - |  157 | `		}` |
|  272 |  158 | `		if( d >= iBase ){` |
|  ! 0 |  159 | `			break;` |
|    - |  160 | `		}` |
|  272 |  161 | `		if( uVal > (SXU64_HIGH - (sxu64)d) / (sxu64)iBase ){` |
|    7 |  162 | `			bOver = 1;` |
|    4 |  163 | `		}else{` |
|  266 |  164 | `			uVal = uVal * (sxu64)iBase + (sxu64)d;` |
|    - |  165 | `		}` |
|  272 |  166 | `		z++;` |
|    2 |  167 | `	}` |
|  114 |  168 | `	if( bUnsigned ){` |
|    - |  169 | `		/* strtoull saturates at its own maximum for EITHER sign, and negates` |
|    - |  170 | ``		 * modulo 2^64 otherwise -- which is why `%u` over "-5" answers`` |
|    - |  171 | `		 * 18446744073709551611. */` |
|    7 |  172 | `		if( bOver ){` |
|    3 |  173 | `			return SXU64_HIGH;` |
|    - |  174 | `		}` |
|    5 |  175 | `		return bNeg ? (sxu64)(0 - uVal) : uVal;` |
|    - |  176 | `	}` |
|  108 |  177 | `	if( bNeg ){` |
|    3 |  178 | `		if( bOver \|\| uVal > (sxu64)SXI64_HIGH + 1 ){` |
|    - |  179 | `			/* PHP_INT_MIN, spelled so no literal has to overflow to reach it. */` |
|    3 |  180 | `			return (sxu64)((sxu64)SXI64_HIGH + 1);` |
|    - |  181 | `		}` |
|  ! 0 |  182 | `		return (sxu64)(0 - uVal);` |
|    - |  183 | `	}` |
|  106 |  184 | `	if( bOver \|\| uVal > (sxu64)SXI64_HIGH ){` |
|    3 |  185 | `		return (sxu64)SXI64_HIGH;` |
|    - |  186 | `	}` |
|  104 |  187 | `	return uVal;` |
|   58 |  188 | `}` |
|    - |  189 | ``/* Render a 64-bit value as unsigned decimal -- php's answer for a `%u` field`` |
|    - |  190 | ` * whose value will not fit a signed int, which it hands back as a STRING. */` |
|    4 |  191 | `static int ScanfUnsignedToStr(sxu64 uVal,char *zOut)` |
|    1 |  192 | `{` |
|    - |  193 | `	char zTmp[24];` |
|    5 |  194 | `	int n = 0, i = 0;` |
|    5 |  195 | `	if( uVal == 0 ){` |
|  ! 0 |  196 | `		zTmp[n++] = '0';` |
|  ! 0 |  197 | `	}` |
|   85 |  198 | `	while( uVal > 0 ){` |
|   81 |  199 | `		zTmp[n++] = (char)('0' + (int)(uVal % 10));` |
|   81 |  200 | `		uVal /= 10;` |
|    1 |  201 | `	}` |
|   85 |  202 | `	while( n > 0 ){` |
|   81 |  203 | `		zOut[i++] = zTmp[--n];` |
|    1 |  204 | `	}` |
|    5 |  205 | `	zOut[i] = 0;` |
|    5 |  206 | `	return i;` |
|    1 |  207 | `}` |
|    - |  208 | `/*` |
|    - |  209 | ``  * Build the character set a `%[` introduces. zSpec points just past the `[` `` |
|    - |  210 | `` * and a closing `]` is guaranteed to be there -- the validation pass refused`` |
|    - |  211 | `` * the format otherwise. Answers the position just past that `]`.`` |
|    - |  212 | ` *` |
|    - |  213 | `` * The walk is php's, quirks included: a `]` or `-` written FIRST is a literal`` |
|    - |  214 | `` * member, a `-` written LAST is a literal member and so is the character`` |
|    - |  215 | ` * before it, and a range written backwards is silently turned around.` |
|    - |  216 | ` */` |
|   20 |  217 | `static const char * ScanfBuildCharSet(scanf_charset *pSet,const char *zSpec,` |
|    - |  218 | `	signed char *aChars,struct scanf_range *aRanges)` |
|    1 |  219 | `{` |
|    - |  220 | `	const char *zCur;` |
|    - |  221 | `	signed char start;` |
|   21 |  222 | `	pSet->exclude = 0;` |
|   21 |  223 | `	pSet->nchars = pSet->nranges = 0;` |
|   21 |  224 | `	pSet->chars = aChars;` |
|   21 |  225 | `	pSet->ranges = aRanges;` |
|   21 |  226 | `	if( zSpec[0] == '^' ){` |
|    5 |  227 | `		pSet->exclude = 1;` |
|    5 |  228 | `		zSpec++;` |
|    2 |  229 | `	}` |
|   21 |  230 | `	zCur = zSpec++;` |
|   21 |  231 | `	start = (signed char)zCur[0];` |
|   21 |  232 | `	if( zCur[0] == ']' \|\| zCur[0] == '-' ){` |
|    5 |  233 | `		pSet->chars[pSet->nchars++] = (signed char)zCur[0];` |
|    5 |  234 | `		zCur = zSpec++;` |
|    2 |  235 | `	}` |
|   61 |  236 | `	while( zCur[0] != ']' ){` |
|   41 |  237 | `		if( zSpec[0] == '-' ){` |
|    - |  238 | `			/* This may open a range, so hold it back rather than listing it. */` |
|   15 |  239 | `			start = (signed char)zCur[0];` |
|   34 |  240 | `		}else if( zCur[0] == '-' ){` |
|   17 |  241 | `			if( zSpec[0] == ']' ){` |
|    - |  242 | `				/* A trailing dash is a member, and so is what came before it. */` |
|    5 |  243 | `				pSet->chars[pSet->nchars++] = start;` |
|    5 |  244 | `				pSet->chars[pSet->nchars++] = (signed char)zCur[0];` |
|    3 |  245 | `			}else{` |
|    - |  246 | `				signed char stop;` |
|   13 |  247 | `				zCur = zSpec++;` |
|   13 |  248 | `				stop = (signed char)zCur[0];` |
|   13 |  249 | `				if( start < stop ){` |
|   11 |  250 | `					pSet->ranges[pSet->nranges].start = start;` |
|   11 |  251 | `					pSet->ranges[pSet->nranges].end = stop;` |
|    6 |  252 | `				}else{` |
|    3 |  253 | `					pSet->ranges[pSet->nranges].start = stop;` |
|    3 |  254 | `					pSet->ranges[pSet->nranges].end = start;` |
|    - |  255 | `				}` |
|   13 |  256 | `				pSet->nranges++;` |
|    - |  257 | `			}` |
|    9 |  258 | `		}else{` |
|   11 |  259 | `			pSet->chars[pSet->nchars++] = (signed char)zCur[0];` |
|    - |  260 | `		}` |
|   41 |  261 | `		zCur = zSpec++;` |
|    1 |  262 | `	}` |
|   21 |  263 | `	return zSpec;` |
|    1 |  264 | `}` |
|   54 |  265 | `static int ScanfCharInSet(const scanf_charset *pSet,int c)` |
|    1 |  266 | `{` |
|   55 |  267 | `	signed char ch = (signed char)c;` |
|   55 |  268 | `	int i, match = 0;` |
|   87 |  269 | `	for( i = 0 ; i < pSet->nchars ; ++i ){` |
|   51 |  270 | `		if( pSet->chars[i] == ch ){` |
|   19 |  271 | `			match = 1;` |
|   19 |  272 | `			break;` |
|    - |  273 | `		}` |
|   17 |  274 | `	}` |
|   55 |  275 | `	if( !match ){` |
|   51 |  276 | `		for( i = 0 ; i < pSet->nranges ; ++i ){` |
|   33 |  277 | `			if( pSet->ranges[i].start <= ch && ch <= pSet->ranges[i].end ){` |
|   19 |  278 | `				match = 1;` |
|   19 |  279 | `				break;` |
|    - |  280 | `			}` |
|    8 |  281 | `		}` |
|   18 |  282 | `	}` |
|   55 |  283 | `	return pSet->exclude ? !match : match;` |
|    1 |  284 | `}` |
|    - |  285 | `/*` |
|    - |  286 | ` * php's "Bad scan conversion character" naming. A format that simply RAN OUT` |
|    - |  287 | ` * lands here with the terminating NUL as the offending character, and php's` |
|    - |  288 | ` * own formatter stops at it -- so that message really does end after the` |
|    - |  289 | ` * opening quote, with no character and no closing one.` |
|    - |  290 | ` */` |
|   20 |  291 | `static sxi32 ScanfBadConversion(ph7_context *pCtx,int c)` |
|    2 |  292 | `{` |
|   22 |  293 | `	if( c == 0 ){` |
|    9 |  294 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  295 | `			"Bad scan conversion character \"");` |
|    - |  296 | `	}` |
|   20 |  297 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|    6 |  298 | `		"Bad scan conversion character \"%c\"",c);` |
|   12 |  299 | `}` |
|    - |  300 | `/*` |
|    - |  301 | ` * Pass one: walk the format, refuse what php refuses, and answer how many` |
|    - |  302 | ` * substitutions it makes.` |
|    - |  303 | ` *` |
|    - |  304 | ` * The assignment COUNTS are what the last two refusals are made of: a` |
|    - |  305 | ` * positional index used twice is "assigned by multiple", and -- when the` |
|    - |  306 | ` * caller passed variables rather than asking for an array -- an index no` |
|    - |  307 | ` * specifier ever names is "not assigned by any", which is php's wording for` |
|    - |  308 | ` * having passed too many variables.` |
|    - |  309 | ` */` |
|  214 |  310 | `static sxi32 ScanfValidateFormat(ph7_context *pCtx,const char *zFmt,int nVar,int *pTotal)` |
|    2 |  311 | `{` |
|    - |  312 | `	int aStatic[16];` |
|  216 |  313 | `	int *aAssign = aStatic;` |
|  216 |  314 | `	int nSpace = (int)SX_ARRAYSIZE(aStatic);` |
|  216 |  315 | `	int gotXpg = 0, gotSequential = 0;` |
|  216 |  316 | `	int objIndex = 0, xpgSize = 0;` |
|    - |  317 | `	int i, nTotal;` |
|  216 |  318 | `	sxi32 rc = SXRET_OK;` |
|  216 |  319 | `	const char *zCur = zFmt;` |
|  216 |  320 | `	if( nVar > nSpace ){` |
|  ! 0 |  321 | `		aAssign = (int *)ph7_context_alloc_chunk(pCtx,(unsigned int)nVar * sizeof(int),1,1);` |
|  ! 0 |  322 | `		if( aAssign == 0 ){` |
|  ! 0 |  323 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |  324 | `		}` |
|  ! 0 |  325 | `		nSpace = nVar;` |
|  ! 0 |  326 | `	}` |
| 3640 |  327 | `	for( i = 0 ; i < nSpace ; ++i ){` |
| 3426 |  328 | `		aAssign[i] = 0;` |
| 1714 |  329 | `	}` |
|  564 |  330 | `	while( zCur[0] != 0 ){` |
|  388 |  331 | `		const char *zCh = zCur++;` |
|  388 |  332 | `		int bSuppress = 0;` |
|  388 |  333 | `		if( zCh[0] != '%' ){` |
|   98 |  334 | `			continue;` |
|    - |  335 | `		}` |
|  292 |  336 | `		zCh = zCur++;` |
|  292 |  337 | `		if( zCh[0] == '%' ){` |
|    3 |  338 | `			continue;` |
|    - |  339 | `		}` |
|  290 |  340 | `		if( zCh[0] == '*' ){` |
|   15 |  341 | `			bSuppress = 1;` |
|   15 |  342 | `			zCh = zCur++;` |
|   15 |  343 | `			goto xpg_done;` |
|    - |  344 | `		}` |
|  276 |  345 | `		if( ScanfIsDigit((unsigned char)zCh[0]) ){` |
|    - |  346 | ``			/* An XPG3 `%n$` specifier, if a `$` closes the run -- and a format`` |
|    - |  347 | `			 * may not carry both spellings. */` |
|    - |  348 | `			const char *zEnd;` |
|   43 |  349 | `			sxu64 uVal = ScanfReadUInt(zCur - 1,&zEnd);` |
|    - |  350 | `			int value;` |
|   43 |  351 | `			if( zEnd[0] != '$' ){` |
|   13 |  352 | `				goto not_xpg;` |
|    - |  353 | `			}` |
|    - |  354 | ``			/* php reads this index through an `int`, so an over-wide run keeps`` |
|    - |  355 | `			 * only its low 32 bits -- reproduced rather than refused. */` |
|   31 |  356 | `			value = (int)(sxu32)uVal;` |
|   31 |  357 | `			zCur = zEnd + 1;` |
|   31 |  358 | `			zCh = zCur++;` |
|   31 |  359 | `			gotXpg = 1;` |
|   31 |  360 | `			if( gotSequential ){` |
|    3 |  361 | `				goto mixed_xpg;` |
|    - |  362 | `			}` |
|   29 |  363 | `			if( value < 1 \|\| (nVar && value > nVar) ){` |
|    4 |  364 | `				goto bad_index;` |
|   27 |  365 | `			}else if( nVar == 0 ){` |
|   23 |  366 | `				if( value > SCANF_MAX_ARGS ){` |
|    3 |  367 | `					goto bad_index;` |
|    - |  368 | `				}` |
|   21 |  369 | `				xpgSize = (xpgSize > value) ? xpgSize : value;` |
|   10 |  370 | `			}` |
|   25 |  371 | `			objIndex = value - 1;` |
|   25 |  372 | `			goto xpg_done;` |
|    - |  373 | `		}` |
|  116 |  374 | `not_xpg:` |
|  246 |  375 | `		gotSequential = 1;` |
|  246 |  376 | `		if( gotXpg ){` |
|    1 |  377 | `mixed_xpg:` |
|    5 |  378 | `			rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  379 | `				"cannot mix \"%%\" and \"%%n$\" conversion specifiers");` |
|    5 |  380 | `			goto done;` |
|    - |  381 | `		}` |
|  121 |  382 | `xpg_done:` |
|    - |  383 | `		/* An optional field width. */` |
|  282 |  384 | `		if( ScanfIsDigit((unsigned char)zCh[0]) ){` |
|   13 |  385 | `			ScanfReadUInt(zCur - 1,&zCur);` |
|   13 |  386 | `			zCh = zCur++;` |
|    6 |  387 | `		}` |
|    - |  388 | `		/* A size specifier is read and ignored, as php's is. */` |
|  282 |  389 | `		if( zCh[0] == 'l' \|\| zCh[0] == 'L' \|\| zCh[0] == 'h' ){` |
|    9 |  390 | `			zCh = zCur++;` |
|    4 |  391 | `		}` |
|  282 |  392 | `		if( !bSuppress && nVar && objIndex >= nVar ){` |
|    3 |  393 | `			goto bad_index;` |
|    - |  394 | `		}` |
|  280 |  395 | `		switch( zCh[0] ){` |
|  115 |  396 | `			case 'n': case 'd': case 'D': case 'i': case 'o':` |
|    - |  397 | `			case 'x': case 'X': case 'u': case 'f': case 'e':` |
|    - |  398 | `			case 'E': case 'g': case 's': case 'c':` |
|  232 |  399 | `				break;` |
|   14 |  400 | `			case '[':` |
|    - |  401 | ``				/* The set has to CLOSE. `^` and a leading `]` are both members`` |
|    - |  402 | `				 * of the spelling rather than the set's end. */` |
|   29 |  403 | `				if( zCur[0] == 0 ){` |
|    3 |  404 | `					goto bad_set;` |
|    - |  405 | `				}` |
|   27 |  406 | `				zCh = zCur++;` |
|   27 |  407 | `				if( zCh[0] == '^' ){` |
|    7 |  408 | `					if( zCur[0] == 0 ){` |
|  ! 0 |  409 | `						goto bad_set;` |
|    - |  410 | `					}` |
|    7 |  411 | `					zCh = zCur++;` |
|    3 |  412 | `				}` |
|   27 |  413 | `				if( zCh[0] == ']' ){` |
|    9 |  414 | `					if( zCur[0] == 0 ){` |
|    5 |  415 | `						goto bad_set;` |
|    - |  416 | `					}` |
|    5 |  417 | `					zCh = zCur++;` |
|    2 |  418 | `				}` |
|   79 |  419 | `				while( zCh[0] != ']' ){` |
|   59 |  420 | `					if( zCur[0] == 0 ){` |
|    3 |  421 | `						goto bad_set;` |
|    - |  422 | `					}` |
|   57 |  423 | `					zCh = zCur++;` |
|    1 |  424 | `				}` |
|   21 |  425 | `				break;` |
|    4 |  426 | `bad_set:` |
|    9 |  427 | `				rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  428 | `					"Unmatched [ in format string");` |
|    9 |  429 | `				goto done;` |
|   10 |  430 | `			default:` |
|   22 |  431 | `				rc = ScanfBadConversion(pCtx,(unsigned char)zCh[0]);` |
|   22 |  432 | `				goto done;` |
|    - |  433 | `		}` |
|  252 |  434 | `		if( !bSuppress ){` |
|  240 |  435 | `			if( objIndex >= nSpace ){` |
|  ! 0 |  436 | `				int nOld = nSpace;` |
|    - |  437 | `				int *aNew;` |
|  ! 0 |  438 | `				nSpace = xpgSize ? xpgSize : (nSpace + (int)SX_ARRAYSIZE(aStatic));` |
|    - |  439 | `				/* Both arms grow past objIndex on every path that reaches here` |
|    - |  440 | `				 * (xpgSize is at least objIndex+1 when it is set at all), but the` |
|    - |  441 | `				 * copy below would run off a SHORTER buffer if one ever did not. */` |
|  ! 0 |  442 | `				if( nSpace < nOld ){` |
|  ! 0 |  443 | `					nSpace = nOld;` |
|  ! 0 |  444 | `				}` |
|  ! 0 |  445 | `				if( objIndex >= nSpace ){` |
|  ! 0 |  446 | `					nSpace = objIndex + 1;` |
|  ! 0 |  447 | `				}` |
|  ! 0 |  448 | `				aNew = (int *)ph7_context_alloc_chunk(pCtx,` |
|  ! 0 |  449 | `					(unsigned int)nSpace * sizeof(int),1,1);` |
|  ! 0 |  450 | `				if( aNew == 0 ){` |
|  ! 0 |  451 | `					rc = PH7_ContextMemoryError(pCtx);` |
|  ! 0 |  452 | `					goto done;` |
|    - |  453 | `				}` |
|  ! 0 |  454 | `				for( i = 0 ; i < nOld ; ++i ){` |
|  ! 0 |  455 | `					aNew[i] = aAssign[i];` |
|  ! 0 |  456 | `				}` |
|  ! 0 |  457 | `				if( aAssign != aStatic ){` |
|  ! 0 |  458 | `					ph7_context_free_chunk(pCtx,aAssign);` |
|  ! 0 |  459 | `				}` |
|  ! 0 |  460 | `				aAssign = aNew;` |
|  ! 0 |  461 | `			}` |
|  240 |  462 | `			aAssign[objIndex]++;` |
|  240 |  463 | `			objIndex++;` |
|  119 |  464 | `		}` |
|    2 |  465 | `	}` |
|  178 |  466 | `	nTotal = nVar;` |
|  178 |  467 | `	if( nTotal == 0 ){` |
|  154 |  468 | `		nTotal = xpgSize ? xpgSize : objIndex;` |
|   76 |  469 | `	}` |
|  178 |  470 | `	if( pTotal ){` |
|  178 |  471 | `		*pTotal = nTotal;` |
|   88 |  472 | `	}` |
|  422 |  473 | `	for( i = 0 ; i < nTotal && i < nSpace ; ++i ){` |
|  250 |  474 | `		if( aAssign[i] > 1 ){` |
|    3 |  475 | `			rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  476 | `				"Variable is assigned by multiple \"%%n$\" conversion specifiers");` |
|    3 |  477 | `			goto done;` |
|  248 |  478 | `		}else if( !xpgSize && aAssign[i] == 0 ){` |
|    3 |  479 | `			rc = PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  480 | `				"Variable is not assigned by any conversion specifiers");` |
|    3 |  481 | `			goto done;` |
|    - |  482 | `		}` |
|  124 |  483 | `	}` |
|  174 |  484 | `	if( aAssign != aStatic ){` |
|  ! 0 |  485 | `		ph7_context_free_chunk(pCtx,aAssign);` |
|  ! 0 |  486 | `	}` |
|  174 |  487 | `	return SXRET_OK;` |
|    3 |  488 | `bad_index:` |
|    7 |  489 | `	rc = gotXpg` |
|    4 |  490 | `		? PH7_VmThrowException(pCtx,"ValueError","\"%%n$\" argument index out of range")` |
|    4 |  491 | `		: PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  492 | `			"Different numbers of variable names and field specifiers");` |
|   21 |  493 | `done:` |
|   44 |  494 | `	if( aAssign != aStatic ){` |
|  ! 0 |  495 | `		ph7_context_free_chunk(pCtx,aAssign);` |
|  ! 0 |  496 | `	}` |
|   44 |  497 | `	return rc == SXRET_OK ? SXERR_ABORT : rc;` |
|  109 |  498 | `}` |
|    - |  499 | `/* Hand one converted value to whichever sink this call has. */` |
|  186 |  500 | `static void ScanfAssign(scanf_sink *pSink,int objIndex,ph7_value *pVal)` |
|    2 |  501 | `{` |
|  188 |  502 | `	if( pSink->nVar > 0 ){` |
|   40 |  503 | `		if( objIndex >= 0 && objIndex < pSink->nVar ){` |
|   40 |  504 | `			PH7_VmStoreArgByRef(pSink->pCtx->pVm,pSink->apVar[objIndex],pVal);` |
|   21 |  505 | `		}` |
|  169 |  506 | `	}else if( pSink->pArray ){` |
|  150 |  507 | `		ph7_array_add_intkey_elem(pSink->pArray,objIndex,pVal);` |
|   74 |  508 | `	}` |
|  188 |  509 | `}` |
|  114 |  510 | `static void ScanfAssignInt(scanf_sink *pSink,int objIndex,sxi64 iVal)` |
|    2 |  511 | `{` |
|  116 |  512 | `	ph7_value_int64(pSink->pTmp,iVal);` |
|  116 |  513 | `	ScanfAssign(pSink,objIndex,pSink->pTmp);` |
|  116 |  514 | `}` |
|   58 |  515 | `static void ScanfAssignStr(scanf_sink *pSink,int objIndex,const char *zStr,int nLen)` |
|    2 |  516 | `{` |
|    - |  517 | `	/* ph7_value_string() APPENDS, so the scratch has to be emptied first --` |
|    - |  518 | `	 * the same slot carries every field this call converts. */` |
|   60 |  519 | `	ph7_value_string(pSink->pTmp,"",0);` |
|   60 |  520 | `	ph7_value_reset_string_cursor(pSink->pTmp);` |
|   60 |  521 | `	ph7_value_string(pSink->pTmp,zStr,nLen);` |
|   60 |  522 | `	ScanfAssign(pSink,objIndex,pSink->pTmp);` |
|   60 |  523 | `}` |
|    - |  524 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   14 |  525 | `static void ScanfAssignDouble(scanf_sink *pSink,int objIndex,double rVal)` |
|    1 |  526 | `{` |
|   15 |  527 | `	ph7_value_double(pSink->pTmp,rVal);` |
|   15 |  528 | `	ScanfAssign(pSink,objIndex,pSink->pTmp);` |
|   15 |  529 | `}` |
|    - |  530 | `#endif` |
|    - |  531 | `/*` |
|    - |  532 | ` * Pass two: scan zStr with zFmt and fill the sink.` |
|    - |  533 | ` *` |
|    - |  534 | ` * The whole family's return value is decided here: how many conversions were` |
|    - |  535 | ` * PERFORMED, or the EOF answer when the input ran out before any of them did.` |
|    - |  536 | ` */` |
|  172 |  537 | `static sxi32 ScanfExecFormat(ph7_context *pCtx,const char *zStr,const char *zFmt,` |
|    - |  538 | `	scanf_sink *pSink,int *pnConv,int *pbEof)` |
|    2 |  539 | `{` |
|  174 |  540 | `	const char *zBase = zStr;` |
|  174 |  541 | `	const char *zCur = zFmt;` |
|  174 |  542 | `	int objIndex = 0;` |
|  174 |  543 | `	int nConv = 0, bUnderflow = 0;` |
|  174 |  544 | `	int op = 0, iBase = 0;` |
|  174 |  545 | `	*pbEof = 0;` |
|  438 |  546 | `	while( zCur[0] != 0 ){` |
|  306 |  547 | `		const char *zCh = zCur++;` |
|  306 |  548 | `		int flags = 0;` |
|    - |  549 | `		int sch;` |
|    - |  550 | `		sxu64 width;` |
|  306 |  551 | `		if( ScanfIsSpace((unsigned char)zCh[0]) ){` |
|    - |  552 | `			/* Whitespace in the format eats whatever whitespace is there. */` |
|  102 |  553 | `			while( ScanfIsSpace((unsigned char)zStr[0]) ){` |
|   50 |  554 | `				if( zStr[0] == 0 ){` |
|  ! 0 |  555 | `					goto done;` |
|    - |  556 | `				}` |
|   50 |  557 | `				zStr++;` |
|    2 |  558 | `			}` |
|   54 |  559 | `			continue;` |
|    - |  560 | `		}` |
|  254 |  561 | `		if( zCh[0] != '%' ){` |
|   10 |  562 | `literal:` |
|   23 |  563 | `			if( zStr[0] == 0 ){` |
|    3 |  564 | `				bUnderflow = 1;` |
|    3 |  565 | `				goto done;` |
|    - |  566 | `			}` |
|   21 |  567 | `			sch = zStr[0];` |
|   21 |  568 | `			zStr++;` |
|   21 |  569 | `			if( zCh[0] != sch ){` |
|    5 |  570 | `				goto done;` |
|    - |  571 | `			}` |
|   17 |  572 | `			continue;` |
|    - |  573 | `		}` |
|  234 |  574 | `		zCh = zCur++;` |
|  234 |  575 | `		if( zCh[0] == '%' ){` |
|    3 |  576 | `			goto literal;` |
|    - |  577 | `		}` |
|  232 |  578 | `		if( zCh[0] == '*' ){` |
|   13 |  579 | `			flags \|= SCANF_SUPPRESS;` |
|   13 |  580 | `			zCh = zCur++;` |
|  226 |  581 | `		}else if( ScanfIsDigit((unsigned char)zCh[0]) ){` |
|    - |  582 | `			const char *zEnd;` |
|   31 |  583 | `			sxu64 uVal = ScanfReadUInt(zCur - 1,&zEnd);` |
|   31 |  584 | `			if( zEnd[0] == '$' ){` |
|   19 |  585 | `				zCur = zEnd + 1;` |
|   19 |  586 | `				zCh = zCur++;` |
|   19 |  587 | `				objIndex = (int)(sxu32)uVal - 1;` |
|    9 |  588 | `			}` |
|   15 |  589 | `		}` |
|  232 |  590 | `		if( ScanfIsDigit((unsigned char)zCh[0]) ){` |
|   13 |  591 | `			width = ScanfReadUInt(zCur - 1,&zCur);` |
|   13 |  592 | `			zCh = zCur++;` |
|    7 |  593 | `		}else{` |
|  220 |  594 | `			width = 0;` |
|    - |  595 | `		}` |
|  232 |  596 | `		if( zCh[0] == 'l' \|\| zCh[0] == 'L' \|\| zCh[0] == 'h' ){` |
|    7 |  597 | `			zCh = zCur++;` |
|    3 |  598 | `		}` |
|  232 |  599 | `		switch( zCh[0] ){` |
|    3 |  600 | `			case 'n':` |
|    7 |  601 | `				if( (flags & SCANF_SUPPRESS) == 0 ){` |
|    7 |  602 | `					ScanfAssignInt(pSink,objIndex++,(sxi64)(zStr - zBase));` |
|    3 |  603 | `				}` |
|    7 |  604 | `				nConv++;` |
|    7 |  605 | `				continue;` |
|   61 |  606 | `			case 'd': case 'D':` |
|  124 |  607 | `				op = 'i'; iBase = 10;` |
|  124 |  608 | `				break;` |
|    2 |  609 | `			case 'i':` |
|    5 |  610 | `				op = 'i'; iBase = 0;` |
|    5 |  611 | `				break;` |
|    1 |  612 | `			case 'o':` |
|    3 |  613 | `				op = 'i'; iBase = 8;` |
|    3 |  614 | `				break;` |
|    3 |  615 | `			case 'x': case 'X':` |
|    7 |  616 | `				op = 'i'; iBase = 16;` |
|    7 |  617 | `				break;` |
|    3 |  618 | `			case 'u':` |
|    7 |  619 | `				op = 'i'; iBase = 10;` |
|    7 |  620 | `				flags \|= SCANF_UNSIGNED;` |
|    7 |  621 | `				break;` |
|    9 |  622 | `			case 'f': case 'e': case 'E': case 'g':` |
|   19 |  623 | `				op = 'f';` |
|   19 |  624 | `				break;` |
|   18 |  625 | `			case 's':` |
|   38 |  626 | `				op = 's';` |
|   38 |  627 | `				break;` |
|    5 |  628 | `			case 'c':` |
|    - |  629 | `				/* php differs from Tcl here and allows a width; without one it` |
|    - |  630 | `				 * reads a single character, and it never skips whitespace. */` |
|   11 |  631 | `				op = 's';` |
|   11 |  632 | `				flags \|= SCANF_NOSKIP;` |
|   11 |  633 | `				if( width == 0 ){` |
|    7 |  634 | `					width = 1;` |
|    3 |  635 | `				}` |
|   11 |  636 | `				break;` |
|   10 |  637 | `			case '[':` |
|   21 |  638 | `				op = '[';` |
|   21 |  639 | `				flags \|= SCANF_NOSKIP;` |
|   21 |  640 | `				break;` |
|  ! 0 |  641 | `			default:` |
|    - |  642 | `				/* Unreachable: the validation pass refused every other byte. */` |
|  ! 0 |  643 | `				continue;` |
|    - |  644 | `		}` |
|  226 |  645 | `		if( zStr[0] == 0 ){` |
|   16 |  646 | `			bUnderflow = 1;` |
|   16 |  647 | `			goto done;` |
|    - |  648 | `		}` |
|  212 |  649 | `		if( (flags & SCANF_NOSKIP) == 0 ){` |
|  186 |  650 | `			while( zStr[0] != 0 && ScanfIsSpace((unsigned char)zStr[0]) ){` |
|    5 |  651 | `				zStr++;` |
|    1 |  652 | `			}` |
|  182 |  653 | `			if( zStr[0] == 0 ){` |
|    5 |  654 | `				bUnderflow = 1;` |
|    5 |  655 | `				goto done;` |
|    - |  656 | `			}` |
|   88 |  657 | `		}` |
|  208 |  658 | `		switch( op ){` |
|   21 |  659 | `			case 's': {` |
|    - |  660 | `				const char *zEnd;` |
|   44 |  661 | `				if( width == 0 ){` |
|   32 |  662 | `					width = SXU64_HIGH;` |
|   15 |  663 | `				}` |
|   44 |  664 | `				zEnd = zStr;` |
|  154 |  665 | `				while( zEnd[0] != 0 ){` |
|  142 |  666 | `					if( ScanfIsSpace((unsigned char)zEnd[0]) ){` |
|   24 |  667 | `						break;` |
|    - |  668 | `					}` |
|  120 |  669 | `					zEnd++;` |
|  120 |  670 | `					if( --width == 0 ){` |
|    9 |  671 | `						break;` |
|    - |  672 | `					}` |
|    2 |  673 | `				}` |
|   44 |  674 | `				if( (flags & SCANF_SUPPRESS) == 0 ){` |
|   40 |  675 | `					ScanfAssignStr(pSink,objIndex++,zStr,(int)(zEnd - zStr));` |
|   19 |  676 | `				}` |
|   44 |  677 | `				zStr = zEnd;` |
|   44 |  678 | `				break;` |
|    - |  679 | `			}` |
|   10 |  680 | `			case '[': {` |
|    - |  681 | `				scanf_charset sSet;` |
|    - |  682 | `				const char *zEnd;` |
|    - |  683 | `				signed char *aChars;` |
|    - |  684 | `				struct scanf_range *aRanges;` |
|   21 |  685 | `				const char *zSpec = zCur;` |
|    - |  686 | `				sxu32 nSpec;` |
|    - |  687 | ``				/* Measure the spec the way the validation pass read it: `^` and`` |
|    - |  688 | ``				 * a `]` written FIRST belong to the spelling, so the closing`` |
|    - |  689 | `				 * bracket is the first one past them. The pass has already` |
|    - |  690 | `				 * proved one is there. */` |
|   21 |  691 | `				if( zSpec[0] == '^' ){` |
|    5 |  692 | `					zSpec++;` |
|    2 |  693 | `				}` |
|   21 |  694 | `				if( zSpec[0] == ']' ){` |
|    5 |  695 | `					zSpec++;` |
|    2 |  696 | `				}` |
|   73 |  697 | `				while( zSpec[0] != ']' ){` |
|   53 |  698 | `					zSpec++;` |
|    1 |  699 | `				}` |
|   21 |  700 | `				nSpec = (sxu32)(zSpec - zCur) + 1;` |
|    - |  701 | `				/* Two members can come out of one step (a trailing dash lists` |
|    - |  702 | `				 * both itself and what preceded it), so leave room for it. */` |
|   31 |  703 | `				aChars = (signed char *)ph7_context_alloc_chunk(pCtx,` |
|   10 |  704 | `					(nSpec + 2) * (sxu32)sizeof(signed char),1,1);` |
|   31 |  705 | `				aRanges = (struct scanf_range *)ph7_context_alloc_chunk(pCtx,` |
|   20 |  706 | `					(nSpec + 1) * (sxu32)sizeof(struct scanf_range),1,1);` |
|   21 |  707 | `				if( aChars == 0 \|\| aRanges == 0 ){` |
|  ! 0 |  708 | `					return PH7_ContextMemoryError(pCtx);` |
|    - |  709 | `				}` |
|   21 |  710 | `				if( width == 0 ){` |
|   21 |  711 | `					width = SXU64_HIGH;` |
|   10 |  712 | `				}` |
|   21 |  713 | `				zEnd = zStr;` |
|   21 |  714 | `				zCur = ScanfBuildCharSet(&sSet,zCur,aChars,aRanges);` |
|   59 |  715 | `				while( zEnd[0] != 0 ){` |
|   55 |  716 | `					if( !ScanfCharInSet(&sSet,(unsigned char)zEnd[0]) ){` |
|   17 |  717 | `						break;` |
|    - |  718 | `					}` |
|   39 |  719 | `					zEnd++;` |
|   39 |  720 | `					if( --width == 0 ){` |
|  ! 0 |  721 | `						break;` |
|    - |  722 | `					}` |
|    1 |  723 | `				}` |
|   21 |  724 | `				ph7_context_free_chunk(pCtx,aChars);` |
|   21 |  725 | `				ph7_context_free_chunk(pCtx,aRanges);` |
|   21 |  726 | `				if( zStr == zEnd ){` |
|    - |  727 | `					/* Nothing in the set is here: the scan stops. */` |
|    5 |  728 | `					goto done;` |
|    - |  729 | `				}` |
|   17 |  730 | `				if( (flags & SCANF_SUPPRESS) == 0 ){` |
|   17 |  731 | `					ScanfAssignStr(pSink,objIndex++,zStr,(int)(zEnd - zStr));` |
|    8 |  732 | `				}` |
|   17 |  733 | `				zStr = zEnd;` |
|   17 |  734 | `				break;` |
|    - |  735 | `			}` |
|   63 |  736 | `			case 'i': {` |
|    - |  737 | `				char zBuf[SCANF_NUM_BUF];` |
|  128 |  738 | `				char *zOut = zBuf;` |
|    - |  739 | `				sxu64 uVal;` |
|  128 |  740 | `				zBuf[0] = 0;` |
|  128 |  741 | `				if( width == 0 \|\| width > SCANF_NUM_BUF - 1 ){` |
|  124 |  742 | `					width = SCANF_NUM_BUF - 1;` |
|   61 |  743 | `				}` |
|  128 |  744 | `				flags \|= SCANF_SIGNOK \| SCANF_NODIGITS \| SCANF_NOZERO;` |
|  364 |  745 | `				for( ; width > 0 ; width-- ){` |
|  362 |  746 | `					int c = (unsigned char)zStr[0];` |
|  362 |  747 | `					switch( c ){` |
|   13 |  748 | `						case '0':` |
|   27 |  749 | `							if( iBase == 16 ){` |
|    7 |  750 | `								flags \|= SCANF_XOK;` |
|    3 |  751 | `							}` |
|   27 |  752 | `							if( iBase == 0 ){` |
|    5 |  753 | `								iBase = 8;` |
|    5 |  754 | `								flags \|= SCANF_XOK;` |
|    2 |  755 | `							}` |
|   27 |  756 | `							if( flags & SCANF_NOZERO ){` |
|   21 |  757 | `								flags &= ~(SCANF_SIGNOK \| SCANF_NODIGITS \| SCANF_NOZERO);` |
|   11 |  758 | `							}else{` |
|    7 |  759 | `								flags &= ~(SCANF_SIGNOK \| SCANF_XOK \| SCANF_NODIGITS);` |
|    - |  760 | `							}` |
|   27 |  761 | `							goto add_int;` |
|   63 |  762 | `						case '1': case '2': case '3': case '4':` |
|    - |  763 | `						case '5': case '6': case '7':` |
|  128 |  764 | `							if( iBase == 0 ){` |
|  ! 0 |  765 | `								iBase = 10;` |
|  ! 0 |  766 | `							}` |
|  128 |  767 | `							flags &= ~(SCANF_SIGNOK \| SCANF_XOK \| SCANF_NODIGITS);` |
|  128 |  768 | `							goto add_int;` |
|   61 |  769 | `						case '8': case '9':` |
|  123 |  770 | `							if( iBase == 0 ){` |
|  ! 0 |  771 | `								iBase = 10;` |
|  ! 0 |  772 | `							}` |
|  123 |  773 | `							if( iBase <= 8 ){` |
|  ! 0 |  774 | `								break;` |
|    - |  775 | `							}` |
|  123 |  776 | `							flags &= ~(SCANF_SIGNOK \| SCANF_XOK \| SCANF_NODIGITS);` |
|  123 |  777 | `							goto add_int;` |
|    7 |  778 | `						case 'A': case 'B': case 'C':` |
|    - |  779 | `						case 'D': case 'E': case 'F':` |
|    - |  780 | `						case 'a': case 'b': case 'c':` |
|    - |  781 | `						case 'd': case 'e': case 'f':` |
|   16 |  782 | `							if( iBase <= 10 ){` |
|   10 |  783 | `								break;` |
|    - |  784 | `							}` |
|    7 |  785 | `							flags &= ~(SCANF_SIGNOK \| SCANF_XOK \| SCANF_NODIGITS);` |
|    7 |  786 | `							goto add_int;` |
|    7 |  787 | `						case '+': case '-':` |
|   15 |  788 | `							if( flags & SCANF_SIGNOK ){` |
|   11 |  789 | `								flags &= ~SCANF_SIGNOK;` |
|   11 |  790 | `								goto add_int;` |
|    - |  791 | `							}` |
|    5 |  792 | `							break;` |
|    5 |  793 | `						case 'x': case 'X':` |
|   11 |  794 | `							if( (flags & SCANF_XOK) && zOut == zBuf + 1 ){` |
|    7 |  795 | `								iBase = 16;` |
|    7 |  796 | `								flags &= ~SCANF_XOK;` |
|    7 |  797 | `								goto add_int;` |
|    - |  798 | `							}` |
|    4 |  799 | `							break;` |
|   24 |  800 | `						default:` |
|   48 |  801 | `							break;` |
|    - |  802 | `					}` |
|   66 |  803 | `					break;` |
|  148 |  804 | `add_int:` |
|  298 |  805 | `					*zOut++ = *zStr++;` |
|  298 |  806 | `					if( zStr[0] == 0 ){` |
|   61 |  807 | `						break;` |
|    - |  808 | `					}` |
|  120 |  809 | `				}` |
|  128 |  810 | `				if( flags & SCANF_NODIGITS ){` |
|    - |  811 | `					/* A sign and nothing else: this conversion never happened. */` |
|   10 |  812 | `					if( zStr[0] == 0 ){` |
|  ! 0 |  813 | `						bUnderflow = 1;` |
|  ! 0 |  814 | `					}` |
|   10 |  815 | `					goto done;` |
|  120 |  816 | `				}else if( zOut[-1] == 'x' \|\| zOut[-1] == 'X' ){` |
|    - |  817 | ``					/* A `0x` whose digits never arrived: give the x back. */`` |
|    3 |  818 | `					zOut--;` |
|    3 |  819 | `					zStr--;` |
|    1 |  820 | `				}` |
|  120 |  821 | `				*zOut = 0;` |
|  120 |  822 | `				if( (flags & SCANF_SUPPRESS) == 0 ){` |
|   58 |  823 | `					uVal = ScanfStrToNum(zBuf,iBase ? iBase : 10,` |
|  112 |  824 | `						(flags & SCANF_UNSIGNED) != 0);` |
|  116 |  825 | `					if( (flags & SCANF_UNSIGNED) && (sxi64)uVal < 0 ){` |
|    - |  826 | `						char zNum[24];` |
|    5 |  827 | `						int nNum = ScanfUnsignedToStr(uVal,zNum);` |
|    5 |  828 | `						ScanfAssignStr(pSink,objIndex++,zNum,nNum);` |
|    3 |  829 | `					}else{` |
|  110 |  830 | `						ScanfAssignInt(pSink,objIndex++,(sxi64)uVal);` |
|    - |  831 | `					}` |
|   56 |  832 | `				}` |
|  120 |  833 | `				break;` |
|    - |  834 | `			}` |
|    9 |  835 | `			case 'f': {` |
|    - |  836 | `				char zBuf[SCANF_NUM_BUF];` |
|   19 |  837 | `				char *zOut = zBuf;` |
|   19 |  838 | `				zBuf[0] = 0;` |
|   19 |  839 | `				if( width == 0 \|\| width > SCANF_NUM_BUF - 1 ){` |
|   19 |  840 | `					width = SCANF_NUM_BUF - 1;` |
|    9 |  841 | `				}` |
|   19 |  842 | `				flags \|= SCANF_SIGNOK \| SCANF_NODIGITS \| SCANF_PTOK \| SCANF_EXPOK;` |
|   51 |  843 | `				for( ; width > 0 ; width-- ){` |
|   51 |  844 | `					int c = (unsigned char)zStr[0];` |
|   51 |  845 | `					switch( c ){` |
|   13 |  846 | `						case '0': case '1': case '2': case '3': case '4':` |
|    - |  847 | `						case '5': case '6': case '7': case '8': case '9':` |
|   27 |  848 | `							flags &= ~(SCANF_SIGNOK \| SCANF_NODIGITS);` |
|   27 |  849 | `							goto add_float;` |
|    1 |  850 | `						case '+': case '-':` |
|    3 |  851 | `							if( flags & SCANF_SIGNOK ){` |
|    3 |  852 | `								flags &= ~SCANF_SIGNOK;` |
|    3 |  853 | `								goto add_float;` |
|    - |  854 | `							}` |
|  ! 0 |  855 | `							break;` |
|    6 |  856 | `						case '.':` |
|   13 |  857 | `							if( flags & SCANF_PTOK ){` |
|   13 |  858 | `								flags &= ~(SCANF_SIGNOK \| SCANF_PTOK);` |
|   13 |  859 | `								goto add_float;` |
|    - |  860 | `							}` |
|  ! 0 |  861 | `							break;` |
|    4 |  862 | `						case 'e': case 'E':` |
|    - |  863 | `							/* An exponent needs a digit ahead of it. */` |
|    9 |  864 | `							if( (flags & (SCANF_NODIGITS \| SCANF_EXPOK)) == SCANF_EXPOK ){` |
|   10 |  865 | `								flags = (flags & ~(SCANF_EXPOK \| SCANF_PTOK))` |
|    6 |  866 | `									\| SCANF_SIGNOK \| SCANF_NODIGITS;` |
|    7 |  867 | `								goto add_float;` |
|    - |  868 | `							}` |
|    2 |  869 | `							break;` |
|    1 |  870 | `						default:` |
|    2 |  871 | `							break;` |
|    - |  872 | `					}` |
|    5 |  873 | `					break;` |
|   23 |  874 | `add_float:` |
|   47 |  875 | `					*zOut++ = *zStr++;` |
|   47 |  876 | `					if( zStr[0] == 0 ){` |
|   15 |  877 | `						break;` |
|    - |  878 | `					}` |
|   17 |  879 | `				}` |
|   19 |  880 | `				if( flags & SCANF_NODIGITS ){` |
|    9 |  881 | `					if( flags & SCANF_EXPOK ){` |
|    - |  882 | `						/* Not one digit anywhere: the scan stops here. */` |
|    5 |  883 | `						if( zStr[0] == 0 ){` |
|    3 |  884 | `							bUnderflow = 1;` |
|    1 |  885 | `						}` |
|    5 |  886 | `						goto done;` |
|    - |  887 | `					}` |
|    - |  888 | `					/* An exponent that never got its digits: give it back,` |
|    - |  889 | `					 * and its sign with it. */` |
|    5 |  890 | `					zOut--;` |
|    5 |  891 | `					zStr--;` |
|    5 |  892 | `					if( zOut[0] != 'e' && zOut[0] != 'E' ){` |
|    3 |  893 | `						zOut--;` |
|    3 |  894 | `						zStr--;` |
|    1 |  895 | `					}` |
|    2 |  896 | `				}` |
|   15 |  897 | `				*zOut = 0;` |
|   15 |  898 | `				if( (flags & SCANF_SUPPRESS) == 0 ){` |
|    - |  899 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   15 |  900 | `					ScanfAssignDouble(pSink,objIndex++,strtod(zBuf,0));` |
|    - |  901 | `#else` |
|    - |  902 | `					ScanfAssignInt(pSink,objIndex++,0);` |
|    - |  903 | `#endif` |
|    7 |  904 | `				}` |
|   14 |  905 | `				break;` |
|    - |  906 | `			}` |
|  ! 0 |  907 | `			default:` |
|  ! 0 |  908 | `				break;` |
|    - |  909 | `		}` |
|  192 |  910 | `		nConv++;` |
|    2 |  911 | `	}` |
|   66 |  912 | `done:` |
|  174 |  913 | `	*pnConv = nConv;` |
|  174 |  914 | `	*pbEof = (bUnderflow && nConv == 0);` |
|  174 |  915 | `	return SXRET_OK;` |
|   88 |  916 | `}` |
|    - |  917 | `/*` |
|    - |  918 | ` * The body sscanf() and fscanf() share: validate, scan, and answer.` |
|    - |  919 | ` *` |
|    - |  920 | ` * With variables passed the answer is the number of conversions, or -1 when` |
|    - |  921 | ` * the input ran out with none of them made. With none passed it is the array` |
|    - |  922 | ` * of values, whose length the FORMAT decides -- a conversion that never` |
|    - |  923 | ` * happened leaves its NULL in place -- or NULL for that same EOF case.` |
|    - |  924 | ` */` |
|  214 |  925 | `PH7_PRIVATE sxi32 PH7_ScanfRun(ph7_context *pCtx,const char *zStr,int nStr,` |
|    - |  926 | `	const char *zFmt,int nFmt,ph7_value **apVar,int nVar)` |
|    2 |  927 | `{` |
|    - |  928 | `	scanf_sink sSink;` |
|  216 |  929 | `	char *zSubject = 0, *zFormat = 0;` |
|  216 |  930 | `	int nTotal = 0, nConv = 0, bEof = 0, i;` |
|    - |  931 | `	sxi32 rc;` |
|  216 |  932 | `	if( nStr < 0 ){ nStr = 0; }` |
|  216 |  933 | `	if( nFmt < 0 ){ nFmt = 0; }` |
|    - |  934 | `	/* Both strings are walked as C strings, php's own limitation. The copies` |
|    - |  935 | `	 * carry two terminators because the walk may step one past the first. */` |
|  216 |  936 | `	zSubject = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nStr + 2,1,1);` |
|  216 |  937 | `	zFormat = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nFmt + 2,1,1);` |
|  216 |  938 | `	if( zSubject == 0 \|\| zFormat == 0 ){` |
|  ! 0 |  939 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  940 | `	}` |
|  216 |  941 | `	if( nStr > 0 ){` |
|  206 |  942 | `		SyMemcpy(zStr,zSubject,(sxu32)nStr);` |
|  102 |  943 | `	}` |
|  216 |  944 | `	if( nFmt > 0 ){` |
|  214 |  945 | `		SyMemcpy(zFmt,zFormat,(sxu32)nFmt);` |
|  106 |  946 | `	}` |
|  216 |  947 | `	rc = ScanfValidateFormat(pCtx,zFormat,nVar,&nTotal);` |
|  216 |  948 | `	if( rc != SXRET_OK ){` |
|   44 |  949 | `		ph7_context_free_chunk(pCtx,zSubject);` |
|   44 |  950 | `		ph7_context_free_chunk(pCtx,zFormat);` |
|   44 |  951 | `		return rc;` |
|    - |  952 | `	}` |
|  174 |  953 | `	sSink.pCtx = pCtx;` |
|  174 |  954 | `	sSink.apVar = apVar;` |
|  174 |  955 | `	sSink.nVar = nVar;` |
|  174 |  956 | `	sSink.pArray = 0;` |
|  174 |  957 | `	sSink.pTmp = ph7_context_new_scalar(pCtx);` |
|  174 |  958 | `	if( sSink.pTmp == 0 ){` |
|  ! 0 |  959 | `		ph7_context_free_chunk(pCtx,zSubject);` |
|  ! 0 |  960 | `		ph7_context_free_chunk(pCtx,zFormat);` |
|  ! 0 |  961 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  962 | `	}` |
|  174 |  963 | `	if( nVar == 0 ){` |
|  152 |  964 | `		sSink.pArray = ph7_context_new_array(pCtx);` |
|  152 |  965 | `		if( sSink.pArray == 0 ){` |
|  ! 0 |  966 | `			ph7_context_free_chunk(pCtx,zSubject);` |
|  ! 0 |  967 | `			ph7_context_free_chunk(pCtx,zFormat);` |
|  ! 0 |  968 | `			return PH7_ContextMemoryError(pCtx);` |
|    - |  969 | `		}` |
|    - |  970 | `		/* php lays the whole answer out as NULLs first, so a conversion that` |
|    - |  971 | `		 * never happens still has its place in the array. */` |
|  152 |  972 | `		ph7_value_null(sSink.pTmp);` |
|  348 |  973 | `		for( i = 0 ; i < nTotal ; ++i ){` |
|  198 |  974 | `			ph7_array_add_intkey_elem(sSink.pArray,i,sSink.pTmp);` |
|  100 |  975 | `		}` |
|   75 |  976 | `	}` |
|  174 |  977 | `	rc = ScanfExecFormat(pCtx,zSubject,zFormat,&sSink,&nConv,&bEof);` |
|  174 |  978 | `	ph7_context_free_chunk(pCtx,zSubject);` |
|  174 |  979 | `	ph7_context_free_chunk(pCtx,zFormat);` |
|  174 |  980 | `	if( rc != SXRET_OK ){` |
|  ! 0 |  981 | `		return rc;` |
|    - |  982 | `	}` |
|  174 |  983 | `	if( bEof ){` |
|   16 |  984 | `		if( nVar > 0 ){` |
|    3 |  985 | `			ph7_result_int(pCtx,-1);` |
|    2 |  986 | `		}else{` |
|   14 |  987 | `			ph7_result_null(pCtx);` |
|    2 |  988 | `		}` |
|  167 |  989 | `	}else if( nVar > 0 ){` |
|   22 |  990 | `		ph7_result_int(pCtx,nConv);` |
|   12 |  991 | `	}else{` |
|  140 |  992 | `		ph7_result_value(pCtx,sSink.pArray);` |
|    - |  993 | `	}` |
|  174 |  994 | `	return SXRET_OK;` |
|  109 |  995 | `}` |
|    - |  996 | `#endif /* !PH7_DISABLE_BUILTIN_FUNC \|\| !PH7_DISABLE_DISK_IO */` |
|    - |  997 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  998 | `/*` |
|    - |  999 | ` * array\|int\|null sscanf(string $string, string $format, mixed &...$vars)` |
|    - | 1000 | ` *  Parse $string according to $format.` |
|    - | 1001 | ` */` |
|  182 | 1002 | `PH7_PRIVATE int PH7_builtin_sscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1003 | `{` |
|    - | 1004 | `	const char *zStr,*zFmt;` |
|  183 | 1005 | `	int nStr = 0, nFmt = 0;` |
|  183 | 1006 | `	if( nArg < 2 ){` |
|    - | 1007 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|  ! 0 | 1008 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1009 | `		return PH7_OK;` |
|    - | 1010 | `	}` |
|  183 | 1011 | `	zStr = ph7_value_to_string(apArg[0],&nStr);` |
|  183 | 1012 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|  183 | 1013 | `	return (int)PH7_ScanfRun(pCtx,zStr,nStr,zFmt,nFmt,&apArg[2],nArg - 2);` |
|   92 | 1014 | `}` |
|    - | 1015 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1016 |  |
