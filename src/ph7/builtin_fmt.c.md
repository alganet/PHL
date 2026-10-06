# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 546/623 lines (87.64%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|       - |    8 | `                      * rounded shortest-representation output) */` |
|       - |    9 | `/*` |
|       - |   10 | ` * Section:` |
|       - |   11 | ` *    printf-style format engine and the sprintf/printf function family.` |
|       - |   12 | ` * Status:` |
|       - |   13 | ` *    Stable.` |
|       - |   14 | ` */` |
|       - |   15 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |   16 | `#define PH7_NEED_FMT_AND_INI 1` |
|       - |   17 | `#endif` |
|       - |   18 | `#ifdef PH7_NEED_FMT_AND_INI` |
|       - |   19 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|       - |   20 | `/*` |
|       - |   21 | `** Conversion types fall into various categories as defined by the` |
|       - |   22 | `** following enumeration.` |
|       - |   23 | `*/` |
|       - |   24 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|       - |   25 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|       - |   26 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|       - |   27 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|       - |   28 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|       - |   29 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|       - |   30 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|       - |   31 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|       - |   32 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|       - |   33 |  |
|       - |   34 | `/*` |
|       - |   35 | `** Allowed values for ph7_fmt_info.flags` |
|       - |   36 | `*/` |
|       - |   37 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|       - |   38 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|       - |   39 | `/*` |
|       - |   40 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|       - |   41 | `** by an instance of the following structure` |
|       - |   42 | `*/` |
|       - |   43 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|       - |   44 | `struct ph7_fmt_info` |
|       - |   45 | `{` |
|       - |   46 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|       - |   47 | `  sxu8 base;     /* The base for radix conversion */` |
|       - |   48 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|       - |   49 | `  sxu8 type;     /* Conversion paradigm */` |
|       - |   50 | `  char *charset; /* The character set for conversion */` |
|       - |   51 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|       - |   52 | `};` |
|       - |   53 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|       - |   54 | ` * the default float->string cast needs it even when this whole formatting` |
|       - |   55 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|       - |   56 | `/*` |
|       - |   57 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|       - |   58 | ` * used conversion types first.` |
|       - |   59 | ` */` |
|       - |   60 | `static const ph7_fmt_info aFmt[] = {` |
|       - |   61 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|       - |   62 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|       - |   63 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|       - |   64 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|       - |   65 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|       - |   66 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|       - |   67 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|       - |   68 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|       - |   69 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|       - |   70 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|       - |   71 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|       - |   72 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|       - |   73 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|       - |   74 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|       - |   75 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|       - |   76 | `   * formats in the C locale, so they behave identically. */` |
|       - |   77 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|       - |   78 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|       - |   79 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|       - |   80 | `};` |
|       - |   81 | `/*` |
|       - |   82 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|       - |   83 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|       - |   84 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|       - |   85 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|       - |   86 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|       - |   87 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|       - |   88 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|       - |   89 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|       - |   90 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|       - |   91 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|       - |   92 | ` * "all valid".)` |
|       - |   93 | ` */` |
|   26446 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|       5 |   95 | `{` |
|   26451 |   96 | `	const char *zEnd = &zIn[nByte];` |
|       - |   97 | `	int c,idx;` |
|  296991 |   98 | `	while( zIn < zEnd ){` |
|  270583 |   99 | `		if( zIn[0] != '%' ){` |
|  199483 |  100 | `			zIn++;` |
|  199483 |  101 | `			continue;` |
|       - |  102 | `		}` |
|   71105 |  103 | `		zIn++; /* jump the percent sign */` |
|       - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|       - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|       - |  106 | `		 * unknown specifier, matching php. */` |
|  101762 |  107 | `		while( zIn < zEnd ){` |
|  101750 |  108 | `			c = zIn[0];` |
|  101750 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   30614 |  110 | `				zIn++;` |
|   30614 |  111 | `				continue;` |
|       - |  112 | `			}` |
|   71141 |  113 | `			if( c=='\'' ){` |
|      49 |  114 | `				zIn++;` |
|      49 |  115 | `				if( zIn < zEnd ){` |
|      49 |  116 | `					zIn++; /* the custom pad character */` |
|      24 |  117 | `				}` |
|      49 |  118 | `				continue;` |
|       - |  119 | `			}` |
|   71093 |  120 | `			break;` |
|     ! 0 |  121 | `		}` |
|       - |  122 | `		/* field width */` |
|  123230 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   52130 |  124 | `			zIn++;` |
|       5 |  125 | `		}` |
|       - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|       - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   71105 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|      50 |  129 | `			zIn++;` |
|      52 |  130 | `			while( zIn < zEnd ){` |
|      50 |  131 | `				c = zIn[0];` |
|      50 |  132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     ! 0 |  133 | `					zIn++;` |
|     ! 0 |  134 | `					continue;` |
|       - |  135 | `				}` |
|      50 |  136 | `				if( c=='\'' ){` |
|       3 |  137 | `					zIn++;` |
|       3 |  138 | `					if( zIn < zEnd ){` |
|       3 |  139 | `						zIn++;` |
|       1 |  140 | `					}` |
|       3 |  141 | `					continue;` |
|       - |  142 | `				}` |
|      48 |  143 | `				break;` |
|     ! 0 |  144 | `			}` |
|      58 |  145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|       9 |  146 | `				zIn++;` |
|       1 |  147 | `			}` |
|      24 |  148 | `		}` |
|       - |  149 | `		/* precision */` |
|   71105 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     325 |  151 | `			zIn++;` |
|     823 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     501 |  153 | `				zIn++;` |
|       3 |  154 | `			}` |
|     161 |  155 | `		}` |
|       - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   71105 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|      11 |  158 | `			zIn++;` |
|       5 |  159 | `		}` |
|   71105 |  160 | `		if( zIn >= zEnd ){` |
|       - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|       - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|      17 |  163 | `			*pbDangling = TRUE;` |
|      17 |  164 | `			return FALSE;` |
|       - |  165 | `		}` |
|   71089 |  166 | `		c = zIn[0];` |
|   71089 |  167 | `		zIn++; /* jump the conversion specifier */` |
|  146678 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|  146656 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   71067 |  170 | `				break;` |
|       - |  171 | `			}` |
|   37487 |  172 | `		}` |
|   71089 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|      23 |  174 | `			*pBad = c; /* unknown specifier */` |
|      23 |  175 | `			return TRUE;` |
|       - |  176 | `		}` |
|       5 |  177 | `	}` |
|   26413 |  178 | `	return FALSE;` |
|   13058 |  179 | `}` |
|       - |  180 | `/*` |
|       - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|       - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|       - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|       - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|       - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|       - |  186 | ` * Returns PH7_OK when the format is valid.` |
|       - |  187 | ` */` |
|   26446 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|       5 |  189 | `{` |
|   26451 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   26451 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|      34 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      11 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|       - |  194 | `	}` |
|   26429 |  195 | `	if( bDangling ){` |
|      17 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  197 | `			"Missing format specifier at end of string");` |
|       - |  198 | `	}` |
|   26413 |  199 | `	return PH7_OK;` |
|   13058 |  200 | `}` |
|       - |  201 | `/*` |
|       - |  202 | ` * Read a run of decimal digits, saturating at PH7_FMT_NUM_CAP rather than` |
|       - |  203 | `` * wrapping: `%99999999999999999999d` must be REPORTED, and a signed overflow on`` |
|       - |  204 | ` * the way to reporting it is undefined behaviour (it used to make the width come` |
|       - |  205 | ` * out negative, or a positional index come out as an ordinary sequential one).` |
|       - |  206 | ` */` |
|       - |  207 | `#define PH7_FMT_NUM_CAP 2147483647` |
|  142976 |  208 | `static int FormatScanNumber(const char **pzIn,const char *zEnd)` |
|       5 |  209 | `{` |
|  142981 |  210 | `	const char *zIn = *pzIn;` |
|  142981 |  211 | `	int v = 0;` |
|  248409 |  212 | `	while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|  105433 |  213 | `		int d = zIn[0]-'0';` |
|       - |  214 | `		/* Tested BEFORE the multiply: a signed overflow is undefined, so a guard` |
|       - |  215 | `		 * that inspects the wrapped result is one an optimiser may delete — and` |
|       - |  216 | ``		 * did, which is how `%2147483648d` slipped past the range check below. */`` |
|  105433 |  217 | `		if( v > (PH7_FMT_NUM_CAP - d)/10 ){` |
|      29 |  218 | `			v = PH7_FMT_NUM_CAP;` |
|      15 |  219 | `		}else{` |
|  105405 |  220 | `			v = v*10 + d;` |
|       - |  221 | `		}` |
|  105433 |  222 | `		zIn++;` |
|       5 |  223 | `	}` |
|  142981 |  224 | `	*pzIn = zIn;` |
|  142981 |  225 | `	return v;` |
|       5 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|       - |  229 | ` * sequential (non-positional) conversion count and the highest positional index` |
|       - |  230 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|       - |  231 | ` *` |
|       - |  232 | ` * php also refuses a specifier over its own SHAPE before it looks at how many` |
|       - |  233 | ` * values it was given: the three numbers it can carry are bounded, and a` |
|       - |  234 | ` * custom-pad flag the string ends on is named in its own right. *pzBad receives` |
|       - |  235 | ` * php's message for the FIRST such specifier — the scan returns there, since` |
|       - |  236 | ` * php's is a single left-to-right pass and the count it would have produced is` |
|       - |  237 | ` * moot once one of these is raised.` |
|       - |  238 | ` */` |
|       - |  239 | `/* The bound each message quotes is PH7_FMT_NUM_CAP above; php spells it out, so` |
|       - |  240 | ` * these do too (its two shapes differ: the positional one names the open` |
|       - |  241 | ` * interval, the other two the closed one). */` |
|       - |  242 | `#define PH7_FMT_BAD_ARGNUM \` |
|       - |  243 | `	"Argument number specifier must be greater than zero and less than 2147483647"` |
|       - |  244 | `#define PH7_FMT_BAD_WIDTH     "Width must be between 0 and 2147483647"` |
|       - |  245 | `#define PH7_FMT_BAD_PRECISION "Precision must be between 0 and 2147483647"` |
|       - |  246 | `#define PH7_FMT_BAD_PAD       "Missing padding character"` |
|   26498 |  247 | `static int FormatRequiredArgs(const char *zIn,int nByte,const char **pzBad)` |
|       5 |  248 | `{` |
|   26503 |  249 | `	const char *zEnd = &zIn[nByte];` |
|   26503 |  250 | `	int c,seq = 0,maxpos = 0;` |
|  297145 |  251 | `	while( zIn < zEnd ){` |
|  270693 |  252 | `		int numVal = 0,pos = 0;` |
|  270693 |  253 | `		if( zIn[0] != '%' ){` |
|  199523 |  254 | `			zIn++;` |
|  199523 |  255 | `			continue;` |
|       - |  256 | `		}` |
|   71175 |  257 | `		zIn++; /* jump the percent sign */` |
|       - |  258 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|  101836 |  259 | `		while( zIn < zEnd ){` |
|  101818 |  260 | `			c = zIn[0];` |
|  101818 |  261 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|   71205 |  262 | `			if( c=='\'' ){` |
|      53 |  263 | `				zIn++;` |
|      53 |  264 | `				if( zIn >= zEnd ){` |
|       - |  265 | `					/* A custom-pad flag the string ends on: php names THAT, not the` |
|       - |  266 | `					 * specifier it also lacks, and it does so before counting. */` |
|       5 |  267 | `					*pzBad = PH7_FMT_BAD_PAD;` |
|       5 |  268 | `					return seq;` |
|       - |  269 | `				}` |
|      49 |  270 | `				zIn++;` |
|      49 |  271 | `				continue;` |
|       - |  272 | `			}` |
|   71153 |  273 | `			break;` |
|     ! 0 |  274 | `		}` |
|       - |  275 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   71171 |  276 | `		numVal = FormatScanNumber(&zIn,zEnd);` |
|   71171 |  277 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|      68 |  278 | `			pos = numVal;` |
|       - |  279 | ``			/* php: `0 < N < 2147483647`, so `%0$s` and `%2147483647$s` are both the`` |
|       - |  280 | `			 * ValueError — the second used to overflow the required-count report to` |
|       - |  281 | `			 * a NEGATIVE number, and anything past it fell back to sequential. */` |
|      68 |  282 | `			if( pos < 1 \|\| pos >= PH7_FMT_NUM_CAP ){` |
|       9 |  283 | `				*pzBad = PH7_FMT_BAD_ARGNUM;` |
|       9 |  284 | `				return seq;` |
|       - |  285 | `			}` |
|      60 |  286 | `			zIn++;` |
|       - |  287 | `			/* flags then width may follow the positional marker */` |
|      62 |  288 | `			while( zIn < zEnd ){` |
|      60 |  289 | `				c = zIn[0];` |
|      60 |  290 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|      60 |  291 | `				if( c=='\'' ){` |
|       5 |  292 | `					zIn++;` |
|       5 |  293 | `					if( zIn >= zEnd ){` |
|       3 |  294 | `						*pzBad = PH7_FMT_BAD_PAD;` |
|       3 |  295 | `						return seq;` |
|       - |  296 | `					}` |
|       3 |  297 | `					zIn++;` |
|       3 |  298 | `					continue;` |
|       - |  299 | `				}` |
|      56 |  300 | `				break;` |
|     ! 0 |  301 | `			}` |
|      58 |  302 | `			numVal = FormatScanNumber(&zIn,zEnd);` |
|      28 |  303 | `		}` |
|   71161 |  304 | `		if( numVal >= PH7_FMT_NUM_CAP ){` |
|       7 |  305 | `			*pzBad = PH7_FMT_BAD_WIDTH;` |
|       7 |  306 | `			return seq;` |
|       - |  307 | `		}` |
|       - |  308 | `		/* precision */` |
|   71155 |  309 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     329 |  310 | `			zIn++;` |
|     329 |  311 | `			if( FormatScanNumber(&zIn,zEnd) >= PH7_FMT_NUM_CAP ){` |
|       5 |  312 | `				*pzBad = PH7_FMT_BAD_PRECISION;` |
|       5 |  313 | `				return seq;` |
|       - |  314 | `			}` |
|     161 |  315 | `		}` |
|       - |  316 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   71151 |  317 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|   71151 |  318 | `		if( zIn >= zEnd ){` |
|       - |  319 | `			/* A dangling '%' still COUNTS as needing a value: php reports` |
|       - |  320 | `			 * sprintf("%") as "2 arguments are required, 1 given" and only` |
|       - |  321 | `			 * raises the missing-specifier ValueError once the count is met. */` |
|      23 |  322 | `			if( pos > 0 ){` |
|       3 |  323 | `				if( pos > maxpos ){ maxpos = pos; }` |
|       2 |  324 | `			}else{` |
|      21 |  325 | `				seq++;` |
|       - |  326 | `			}` |
|      23 |  327 | `			break;` |
|       - |  328 | `		}` |
|   71129 |  329 | `		c = zIn[0];` |
|   71129 |  330 | `		zIn++; /* jump the conversion specifier */` |
|   71129 |  331 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   70923 |  332 | `		if( pos > 0 ){` |
|      56 |  333 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      29 |  334 | `		}else{` |
|   70869 |  335 | `			seq++;` |
|       - |  336 | `		}` |
|       5 |  337 | `	}` |
|   26479 |  338 | `	return seq > maxpos ? seq : maxpos;` |
|   13084 |  339 | `}` |
|       - |  340 | `/*` |
|       - |  341 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|       - |  342 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|       - |  343 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|       - |  344 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|       - |  345 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|       - |  346 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|       - |  347 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|       - |  348 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|       - |  349 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|       - |  350 | ` * when enough.` |
|       - |  351 | ` */` |
|   26498 |  352 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|       5 |  353 | `{` |
|   26503 |  354 | `	const char *zBad = 0;` |
|   26503 |  355 | `	int required = FormatRequiredArgs(zFormat,nByte,&zBad);` |
|   26503 |  356 | `	if( zBad ){` |
|       - |  357 | `		/* php refuses a specifier's own shape before it counts the values, so` |
|       - |  358 | ``		 * `sprintf("%2$s%0$s","a")` and `sprintf("%'","a")` are the ValueError and`` |
|       - |  359 | `		 * not the (also true) ArgumentCountError. */` |
|      25 |  360 | `		return PH7_VmThrowException(pCtx,"ValueError","%s",zBad);` |
|       - |  361 | `	}` |
|   26479 |  362 | `	if( nValues < required ){` |
|      29 |  363 | `		if( bVararg ){` |
|      13 |  364 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       4 |  365 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|       - |  366 | `		}` |
|      31 |  367 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      10 |  368 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|       - |  369 | `	}` |
|   26451 |  370 | `	return PH7_OK;` |
|   13084 |  371 | `}` |
|       - |  372 | `/*` |
|       - |  373 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|       - |  374 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|       - |  375 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|       - |  376 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|       - |  377 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|       - |  378 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|       - |  379 | ` */` |
|       - |  380 | `/*` |
|       - |  381 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|       - |  382 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|       - |  383 | ` */` |
|     ! 0 |  384 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|     ! 0 |  385 | `{` |
|     ! 0 |  386 | `	if( !ph7_value_is_resource(pArg) ){` |
|       - |  387 | `		char zBuf[64];` |
|     ! 0 |  388 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  389 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|     ! 0 |  390 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|       - |  391 | `	}` |
|     ! 0 |  392 | `	return PH7_OK;` |
|     ! 0 |  393 | `}` |
|   26502 |  394 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|       5 |  395 | `{` |
|   26502 |  396 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg)` |
|   26507 |  397 | `	 \|\| PH7_ArgIsUnstringableObject(pArg) ){` |
|       - |  398 | `		char zBuf[64];` |
|     ! 0 |  399 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  400 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|     ! 0 |  401 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|       - |  402 | `	}` |
|   26507 |  403 | `	return PH7_OK;` |
|   13086 |  404 | `}` |
|       - |  405 | `/*` |
|       - |  406 | ` * Format a given string.` |
|       - |  407 | ` * The root program.  All variations call this core.` |
|       - |  408 | ` * INPUTS:` |
|       - |  409 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|       - |  410 | ` *            1. A pointer to the call context.` |
|       - |  411 | ` *            2. A pointer to the list of characters to be output` |
|       - |  412 | ` *               (Note, this list is NOT null terminated.)` |
|       - |  413 | ` *            3. An integer number of characters to be output.` |
|       - |  414 | ` *               (Note: This number might be zero.)` |
|       - |  415 | ` *            4. Upper layer private data.` |
|       - |  416 | ` *   zIn       This is the format string, as in the usual print.` |
|       - |  417 | ` *   apArg     This is a pointer to a list of arguments.` |
|       - |  418 | ` */` |
|   26408 |  419 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|       - |  420 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|       - |  421 | `	ph7_context *pCtx,  /* call context */` |
|       - |  422 | `	const char *zIn,    /* Format string */` |
|       - |  423 | `	int nByte,          /* Format string length */` |
|       - |  424 | `	int nArg,           /* Total argument of the given arguments */` |
|       - |  425 | `	ph7_value **apArg,  /* User arguments */` |
|       - |  426 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|       - |  427 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|       - |  428 | `	)` |
|       5 |  429 | `{` |
|   26413 |  430 | `	char spaces[] = "                                                  ";` |
|       - |  431 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   26413 |  432 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|       - |  433 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|       - |  434 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|       - |  435 | `	int flag_alternateform; /* True if "#" flag is present */` |
|       - |  436 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|       - |  437 | `	int flag_plussign;      /* True if "+" flag is present */` |
|       - |  438 | `	int flag_zeropad;       /* True if the pad character is '0' */` |
|       - |  439 | `	/* php has ONE pad character per specifier, and the LAST pad flag wins:` |
|       - |  440 | `	 * ' ' selects a space (the default), '0' a zero, "'<c>" any byte` |
|       - |  441 | ``	 * (php_formatted_print's single `padding` local). PH7 carried three`` |
|       - |  442 | ``	 * independent flags instead — a `spaces[]` buffer, a `flag_zeropad` and a`` |
|       - |  443 | ``	 * C-style `flag_blanksign` — so "%0 5d" padded with zeros where php pads with`` |
|       - |  444 | `	 * spaces, "%0'x5d" ignored the 'x', and "% d" printed C's space-for-a-positive` |
|       - |  445 | `	 * -sign ("% d" of 5 was " 5" where php answers "5") — php has no such flag. */` |
|       - |  446 | `	char cPad;` |
|       - |  447 | `	ph7_value *pArg;         /* Current processed argument (the scratch COPY below) */` |
|       - |  448 | `	ph7_value *pRawArg;      /* ...and the caller's own value it was copied from */` |
|       - |  449 | ``	int nPos;                /* apArg index a `%N$` selected, or -1 for sequential */`` |
|       - |  450 | `	/* Every conversion below extracts through ph7_value_to_int64 / _to_double /` |
|       - |  451 | `	 * PH7_ValueToStringUV, and all three convert the value they are handed IN` |
|       - |  452 | `	 * PLACE. Handed the caller's own slots that is a write the caller can see, and` |
|       - |  453 | `	 * both ways in were reachable: vsprintf()/vprintf()/vfprintf() pass the` |
|       - |  454 | `	 * ADDRESSES of the $values array's elements (PH7_HashmapValuesToSet), so` |
|       - |  455 | ``	 * `vsprintf("%o %s %b", $a)` retyped $a's elements — int(0), string, int(0) —`` |
|       - |  456 | `	 * where php leaves the array alone; and a POSITIONAL argument reused by a` |
|       - |  457 | `	 * second specifier was read back already converted, so` |
|       - |  458 | ``	 * `sprintf('%1$d\|%1$s', 3.9)` answered "3\|3" for php's "3\|3.9". One scratch`` |
|       - |  459 | `	 * value per call, reloaded per conversion, keeps the formatter read-only.` |
|       - |  460 | `	 * PH7_MemObjLoad points the copy's blob at the source's bytes, so a %s of a` |
|       - |  461 | `	 * long string still costs no copy unless something writes to it. */` |
|       - |  462 | `	ph7_value sScratch;` |
|       - |  463 | `	int bDropDigits;         /* php's explicit-precision-empties-%x/%X/%o/%b rule */` |
|       - |  464 | `	/* A '0'-padded, right-aligned NUMBER puts its sign in front of the padding` |
|       - |  465 | `	 * (php_sprintf_appendstring writes it before the pad run). Holding it here` |
|       - |  466 | `	 * rather than building the zeros into zWorker is what lets the field be wider` |
|       - |  467 | `	 * than the conversion buffer. Zero for every other conversion, including a` |
|       - |  468 | ``	 * `%s` — php passes appendstring `neg = false` there, which is why "%05s" of`` |
|       - |  469 | `	 * "-5" really is "000-5". */` |
|       - |  470 | `	char cLeadSign;` |
|       - |  471 | `	ph7_int64 iVal;` |
|       - |  472 | `	int precision;           /* Precision of the current field */` |
|       - |  473 | ``	/* php only has a precision when a DIGIT follows the '.' (its `expprec`): a bare`` |
|       - |  474 | `` 	 * "%.s" is a precision of zero that nothing consults, so `sprintf("%.s","abc")` `` |
|       - |  475 | ``	 * is "abc" and `sprintf("%.x",42)` is "2a". Reading the dot alone as an`` |
|       - |  476 | `	 * explicit zero truncated both to nothing. */` |
|       - |  477 | `	int bExplicitPrec;` |
|       - |  478 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|       - |  479 | `	int c,rc,n;` |
|   26413 |  480 | `	sxi32 rcRet = SXRET_OK;   /* Status to hand back through the single exit */` |
|   26413 |  481 | `	ph7_value *pThrowArg = 0; /* First not-stringable %s argument; throws at the end */` |
|       - |  482 | `	int length;              /* Length of the field */` |
|       - |  483 | `	int prefix;` |
|       - |  484 | `	sxu8 xtype;              /* Conversion paradigm */` |
|       - |  485 | `	int width;               /* Width of the current field */` |
|       - |  486 | `	int idx;` |
|   26413 |  487 | `	n = (vf == TRUE) ? 0 : 1;` |
|   26413 |  488 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|       - |  489 | `	/* Take the next argument as a scratch COPY: nothing below may write to the` |
|       - |  490 | `	 * caller's value. Answers 0 exactly as the raw form did when the arguments run` |
|       - |  491 | `	 * out (a shortfall is refused by PH7_FormatCheckArgCount before we get here).` |
|       - |  492 | `	 *` |
|       - |  493 | ``	 * A `%N$` specifier reads argument N and leaves the SEQUENTIAL cursor where it`` |
|       - |  494 | ``	 * was — php keeps the two apart (its `currarg` only ever advances for a`` |
|       - |  495 | `	 * specifier that carries no number), and the counting pass above has always` |
|       - |  496 | `	 * modelled it that way. The format loop did not: a positional MOVED the one` |
|       - |  497 | ``	 * cursor, so `sprintf('%1$s\|%s','a','b')` answered "a\|b" for php's "a\|a" and`` |
|       - |  498 | ``	 * `sprintf('%3$s\|%s','a','b','c')` ran off the end of the list it had just`` |
|       - |  499 | `	 * been told was long enough. */` |
|       - |  500 | `#define NEXT_ARG	( pRawArg = (nPos >= 0 ? (nPos < nArg ? apArg[nPos] : 0) \` |
|       - |  501 | `		: (n < nArg ? apArg[n++] : 0)), \` |
|       - |  502 | `	pRawArg ? (PH7_MemObjRelease(&sScratch), \` |
|       - |  503 | `		PH7_MemObjLoad(pRawArg,&sScratch), &sScratch) : 0 )` |
|       - |  504 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|       - |  505 | `	 * (called by every format builtin before this routine), so the specifier set` |
|       - |  506 | `	 * seen here is always valid. */` |
|       - |  507 | `	/* Start the format process */` |
|   48209 |  508 | `	for(;;){` |
|   97469 |  509 | `		zCur = zIn;` |
|  296921 |  510 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  199457 |  511 | `			zIn++;` |
|       5 |  512 | `		}` |
|   97469 |  513 | `		if( zCur < zIn ){` |
|       - |  514 | `			/* Consume chunk verbatim */` |
|   73825 |  515 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   73825 |  516 | `			if( rc != SXRET_OK ){` |
|       - |  517 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|     ! 0 |  518 | `				break;` |
|       - |  519 | `			}` |
|   36542 |  520 | `		}` |
|   97469 |  521 | `		if( zIn >= zEnd ){` |
|       - |  522 | `			/* No more input to process,break immediately */` |
|   26409 |  523 | `			break;` |
|       - |  524 | `		}` |
|       - |  525 | `		/* Find out what flags are present */` |
|   71065 |  526 | `		flag_leftjustify = flag_plussign =` |
|   71060 |  527 | `			flag_alternateform = flag_zeropad = 0;` |
|       - |  528 | `		/* Reset the pad character: a custom pad ('X) from a PREVIOUS specifier must` |
|       - |  529 | `		 * not bleed into this one. php resets it for every specifier. */` |
|   71065 |  530 | `		cPad = ' ';` |
|   71065 |  531 | `		bDropDigits = 0;` |
|   71065 |  532 | `		cLeadSign = 0;` |
|   71065 |  533 | `		nPos = -1;` |
|   71065 |  534 | `		zIn++; /* Jump the precent sign */` |
|   35177 |  535 | `		do{` |
|  101718 |  536 | `			c = zIn[0];` |
|  101718 |  537 | `			switch( c ){` |
|   28086 |  538 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     112 |  539 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      39 |  540 | `			case ' ':   cPad = ' ';               c = 0;   break;` |
|    2382 |  541 | `			case '0':   cPad = '0';               c = 0;   break;` |
|      23 |  542 | `			case '\'':` |
|      47 |  543 | `				zIn++;` |
|      47 |  544 | `				if( zIn < zEnd ){` |
|       - |  545 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|      47 |  546 | `					cPad = zIn[0];` |
|      47 |  547 | `					c = 0;` |
|      23 |  548 | `				}` |
|      46 |  549 | `				break;` |
|   71060 |  550 | `			default:                                       break;` |
|       - |  551 | `			}` |
|  101718 |  552 | `		}while( c==0 && (zIn++ < zEnd) );` |
|       - |  553 | `		/* Get the field width (saturating — see FormatScanNumber) */` |
|   71065 |  554 | `		width = FormatScanNumber(&zIn,zEnd);` |
|   71065 |  555 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|       - |  556 | `			/* Position specifer */` |
|      48 |  557 | `			if( width > 0 ){` |
|      48 |  558 | `				nPos = vf ? width - 1 : width;` |
|      23 |  559 | `			}` |
|      48 |  560 | `			zIn++;` |
|      48 |  561 | `			width = 0;` |
|       - |  562 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|       - |  563 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|       - |  564 | `			 * not just zero-padding. */` |
|      23 |  565 | `			do{` |
|      50 |  566 | `				c = zIn[0];` |
|      50 |  567 | `				switch( c ){` |
|     ! 0 |  568 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     ! 0 |  569 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  570 | `				case ' ':   cPad = ' ';               c = 0;   break;` |
|     ! 0 |  571 | `				case '0':   cPad = '0';               c = 0;   break;` |
|       1 |  572 | `				case '\'':` |
|       3 |  573 | `					zIn++;` |
|       3 |  574 | `					if( zIn < zEnd ){` |
|       3 |  575 | `						cPad = zIn[0];` |
|       3 |  576 | `						c = 0;` |
|       1 |  577 | `					}` |
|       2 |  578 | `					break;` |
|      46 |  579 | `				default:                                       break;` |
|       - |  580 | `				}` |
|      50 |  581 | `			}while( c==0 && (zIn++ < zEnd) );` |
|      48 |  582 | `			width = FormatScanNumber(&zIn,zEnd);` |
|      23 |  583 | `		}` |
|       - |  584 | `		/* No clamp on the WIDTH: it used to be cut to the conversion buffer` |
|       - |  585 | `		 * (PH7_FMT_BUFSIZ-10 = 1014 bytes) because the zero padding was built` |
|       - |  586 | ``		 * INSIDE that buffer, so `sprintf("%%2000d",5)` answered 1014 characters and`` |
|       - |  587 | ``		 * `%%1100s` silently dropped 86 — a fixed-width record coming out short. The`` |
|       - |  588 | `		 * padding is emitted by the output block below, which chunks it and has no` |
|       - |  589 | `		 * such bound; the two zero-pad-into-zWorker sites are what needed the` |
|       - |  590 | `		 * limit, and both are gone (see cLeadSign). */` |
|       - |  591 | `		/* Get the precision */` |
|   71065 |  592 | `		precision = -1;` |
|   71065 |  593 | `		bExplicitPrec = 0;` |
|   71065 |  594 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     325 |  595 | `			zIn++;` |
|     325 |  596 | `			bExplicitPrec = ( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' );` |
|     325 |  597 | `			precision = FormatScanNumber(&zIn,zEnd);` |
|     161 |  598 | `		}` |
|       - |  599 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|       - |  600 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|       - |  601 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   71065 |  602 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|       9 |  603 | `			zIn++;` |
|       4 |  604 | `		}` |
|   71065 |  605 | `		if( zIn >= zEnd ){` |
|       - |  606 | `			/* No more input */` |
|     ! 0 |  607 | `			break;` |
|       - |  608 | `		}` |
|       - |  609 | `		/* Fetch the info entry for the field */` |
|   71065 |  610 | `		pInfo = 0;` |
|   71065 |  611 | `		xtype = PH7_FMT_ERROR;` |
|   71065 |  612 | `		c = zIn[0];` |
|   71065 |  613 | `		zIn++; /* Jump the format specifer */` |
|  146280 |  614 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|  146280 |  615 | `			if( c==aFmt[idx].fmttype ){` |
|   71065 |  616 | `				pInfo = &aFmt[idx];` |
|   71065 |  617 | `				xtype = pInfo->type;` |
|   71065 |  618 | `				break;` |
|       - |  619 | `			}` |
|   37300 |  620 | `		}` |
|   71065 |  621 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   71065 |  622 | `		length = 0;` |
|       - |  623 | `		/* A '0' pad — however it was spelled, "%05d" or the custom "%'05d" — is` |
|       - |  624 | `		 * also php's "put the sign in front of the padding" rule` |
|       - |  625 | ``		 * (php_sprintf_appendstring's `(neg \|\| always_sign) && padding == '0'`),`` |
|       - |  626 | `		 * which is what the two zero-pad blocks below implement. */` |
|   71065 |  627 | `		flag_zeropad = (cPad == '0');` |
|       - |  628 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|       - |  629 | `		 /*` |
|       - |  630 | `		  ** At this point, variables are initialized as follows:` |
|       - |  631 | `		  **` |
|       - |  632 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|       - |  633 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|       - |  634 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|       - |  635 | `		  **                               field width was negative.` |
|       - |  636 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|       - |  637 | `		  **                               the conversion character.` |
|       - |  638 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|       - |  639 | `		  **   width                       The specified field width.  This is` |
|       - |  640 | `		  **                               always non-negative.  Zero is the default.` |
|       - |  641 | `		  **   precision                   The specified precision.  The default` |
|       - |  642 | `		  **                               is -1.` |
|       - |  643 | `		  */` |
|   71065 |  644 | `		switch(xtype){` |
|     103 |  645 | `		case PH7_FMT_PERCENT:` |
|       - |  646 | `			/* A literal percent character */` |
|     208 |  647 | `			zWorker[0] = '%';` |
|     208 |  648 | `			length = (int)sizeof(char);` |
|     208 |  649 | `			break;` |
|       5 |  650 | `		case PH7_FMT_CHARX:` |
|       - |  651 | `			/* The argument is treated as an integer, and presented as the character` |
|       - |  652 | `			 * with that ASCII value` |
|       - |  653 | `			 */` |
|      12 |  654 | `			pArg = NEXT_ARG;` |
|      12 |  655 | `			if( pArg == 0 ){` |
|     ! 0 |  656 | `				c = 0;` |
|     ! 0 |  657 | `			}else{` |
|       - |  658 | `				/* An integer conversion is a CAST site: php warns here for a float` |
|       - |  659 | `				 * no int can hold, then formats the wrapped value. */` |
|      12 |  660 | `				PH7_MemObjWarnIntCast(pArg);` |
|      12 |  661 | `				c = ph7_value_to_int(pArg);` |
|       - |  662 | `			}` |
|       - |  663 | `			/* NUL byte is an acceptable value */` |
|      12 |  664 | `			zWorker[0] = (char)c;` |
|      12 |  665 | `			length = (int)sizeof(char);` |
|       - |  666 | `			/* php's 'c' is the one conversion with no field: it appends the byte` |
|       - |  667 | `			 * through php_sprintf_appendchar, which takes neither a width nor an` |
|       - |  668 | `			 * alignment, so "%5c" and "%-5c" are both a bare one-byte string. */` |
|      12 |  669 | `			width = 0;` |
|      12 |  670 | `			break;` |
|   29246 |  671 | `		case PH7_FMT_STRING:` |
|       - |  672 | `			/* the argument is treated as and presented as a string */` |
|   57872 |  673 | `			pArg = NEXT_ARG;` |
|   57872 |  674 | `			if( pArg == 0 ){` |
|     ! 0 |  675 | `				length = 0;` |
|   57872 |  676 | `			}else if( PH7_MemObjIsNotStringable(pArg) ){` |
|       - |  677 | `				/* php's user-visible coercion for %s, object half: a class with` |
|       - |  678 | `				 * no __toString() is the catchable "could not be converted to` |
|       - |  679 | `				 * string" Error — but php does NOT let it interrupt the format. The` |
|       - |  680 | `				 * conversion substitutes NOTHING, the format runs to the end, the` |
|       - |  681 | `				 * output is written, and only then does the Error surface. So the` |
|       - |  682 | `				 * throw cannot be RAISED here: PHL's VmThrowException runs an` |
|       - |  683 | `				 * in-place catch immediately, which would print the format's tail` |
|       - |  684 | `				 * after the catch body. Remember the value and throw once the` |
|       - |  685 | `				 * output is out (see the tail of this function). */` |
|      21 |  686 | `				zBuf = "";` |
|      21 |  687 | `				length = 0;` |
|      21 |  688 | `				if( pThrowArg == 0 ){` |
|       - |  689 | `					/* The CALLER's value, not the scratch copy: this one is used after` |
|       - |  690 | `					 * the loop, once the scratch has been reloaded (and released). */` |
|      19 |  691 | `					pThrowArg = pRawArg;` |
|       9 |  692 | `				}` |
|      11 |  693 | `			}else{` |
|       - |  694 | `				/* An ARRAY warns and renders as "Array"; a Stringable renders. */` |
|       - |  695 | `				const char *zSv;` |
|   57852 |  696 | `				sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zSv,&length);` |
|   57852 |  697 | `				zBuf = (char *)zSv;` |
|   57852 |  698 | `				if( rcSv != SXRET_OK ){` |
|       - |  699 | `					/* A __toString() that THREW: unlike the case above this one` |
|       - |  700 | `					 * cannot be predicted, and the throw has already run any` |
|       - |  701 | `					 * in-place catch. Stop formatting rather than emitting the` |
|       - |  702 | `					 * format's tail after the catch body — every other builtin that` |
|       - |  703 | `					 * calls user code (array_map, usort) stops the same way. php` |
|       - |  704 | `					 * keeps going and prints the tail; recorded divergence, and` |
|       - |  705 | `					 * both engines raise the same exception. */` |
|       3 |  706 | `					rcRet = rcSv;` |
|       3 |  707 | `					goto Done;` |
|       - |  708 | `				}` |
|       - |  709 | `			}` |
|   57870 |  710 | `			if( length < 1 ){` |
|       - |  711 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|       - |  712 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|       - |  713 | `				 * absent optional part gained a stray space. */` |
|     831 |  714 | `				zBuf = "";` |
|     831 |  715 | `				length = 0;` |
|     413 |  716 | `			}` |
|   57870 |  717 | `			if( bExplicitPrec && precision<length ){` |
|       7 |  718 | `				length = precision;` |
|       3 |  719 | `			}` |
|   57870 |  720 | `			break;` |
|    6334 |  721 | `		case PH7_FMT_RADIX: {` |
|       - |  722 | `			/* The digits are produced from an UNSIGNED accumulator. Two php rules` |
|       - |  723 | `			 * ride on that, and the inherited signed one got both wrong:` |
|       - |  724 | `			 *` |
|       - |  725 | `			 *  - only %d is SIGNED. %u/%x/%X/%o/%b REINTERPRET the same 64 bits as` |
|       - |  726 | `			 *    unsigned (php_sprintf_appenduint / php_sprintf_append2n cast to` |
|       - |  727 | `			 *    zend_ulong), so sprintf("%x",-1) is "ffffffffffffffff", not the` |
|       - |  728 | `			 *    magnitude "1" this used to print for every negative value;` |
|       - |  729 | `			 *  - the magnitude of PHP_INT_MIN has no signed representation, so` |
|       - |  730 | ``			 *    `iVal = -iVal` was signed overflow — undefined, and the guard`` |
|       - |  731 | ``			 *    testing for it afterwards (`if( iVal < 0 )`) is exactly what a`` |
|       - |  732 | `			 *    compiler may assume cannot happen. It did: the negative` |
|       - |  733 | ``			 *    accumulator reached `cset[iVal%base]`, indexing BEFORE the digit`` |
|       - |  734 | `			 *    table, so sprintf("%d",PHP_INT_MIN) printed whatever bytes sat` |
|       - |  735 | `			 *    there. Unsigned negation is well-defined for every input.` |
|       - |  736 | `			 */` |
|       - |  737 | `			sxu64 uVal;` |
|   12592 |  738 | `			pArg = NEXT_ARG;` |
|   12592 |  739 | `			if( pArg == 0 ){` |
|     ! 0 |  740 | `				iVal = 0;` |
|     ! 0 |  741 | `			}else{` |
|       - |  742 | ``				/* Every radix is a CAST site: `%d`/`%x`/`%u`/`%b`/`%o` of a float`` |
|       - |  743 | `				 * no int can hold warn, then print the modular wrap of it. */` |
|   12592 |  744 | `				PH7_MemObjWarnIntCast(pArg);` |
|   12592 |  745 | `				iVal = ph7_value_to_int64(pArg);` |
|       - |  746 | `			}` |
|       - |  747 | `			/* An integer conversion has no PRECISION in php: the '.' part of the` |
|       - |  748 | ``			 * specifier never reaches the digits. `%.5d` of 42 is "42", not the`` |
|       - |  749 | `			 * "00042" C would print — php's php_sprintf_appendint simply is not` |
|       - |  750 | `			 * handed one. For the other radices the same absence is louder:` |
|       - |  751 | `			 * php_sprintf_append2n forwards a max_width of 0 with the` |
|       - |  752 | `			 * "precision was given" flag set, so ANY explicit precision truncates` |
|       - |  753 | ``			 * the digits to nothing and `%.1x` of 42 is the EMPTY string (padded`` |
|       - |  754 | `			 * to $width, which is why "%5.1x" is five spaces). Reproduced rather` |
|       - |  755 | `			 * than smoothed over — parity is binding (the scope policy). */` |
|   12592 |  756 | `			bDropDigits = (bExplicitPrec && pInfo->base != 10);` |
|   12592 |  757 | `			if( precision >= 0 ){` |
|      19 |  758 | `				precision = -1;` |
|       9 |  759 | `			}` |
|       - |  760 | `			/* php's "Can't right-pad 0's on integers" (php_sprintf_appendint, which` |
|       - |  761 | `			 * %u shares) — and only there: %x/%X/%o/%b go through append2n and %e/%f` |
|       - |  762 | `			 * through appenddouble, which both DO right-pad with zeros, so` |
|       - |  763 | `			 * "%-08x" of 5 really is "50000000" while "%-08d" is "5       ".` |
|       - |  764 | `			 * base 10 is exactly the 'd'/'u' pair of the table above. */` |
|   12592 |  765 | `			if( flag_leftjustify && flag_zeropad && pInfo->base == 10 ){` |
|      19 |  766 | `				flag_zeropad = 0;` |
|      19 |  767 | `				cPad = ' ';` |
|       9 |  768 | `			}` |
|       - |  769 | `        /* For the format %#x, the value zero is printed "0" not "0x0". */` |
|   12592 |  770 | `        if( iVal==0 ) flag_alternateform = 0;` |
|   12592 |  771 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|   10102 |  772 | `          if( iVal<0 ){` |
|     517 |  773 | `            uVal = (sxu64)0 - (sxu64)iVal;` |
|     517 |  774 | `            prefix = '-';` |
|     261 |  775 | `          }else{` |
|    9590 |  776 | `            uVal = (sxu64)iVal;` |
|       - |  777 | `            /* php's ' ' is a PAD selector, not C's space-for-a-positive-sign, so` |
|       - |  778 | `             * '+' is the only flag that prefixes a non-negative value. */` |
|    9590 |  779 | `            prefix = flag_plussign ? '+' : 0;` |
|       - |  780 | `          }` |
|    5013 |  781 | `        }else{` |
|    2494 |  782 | `			uVal = (sxu64)iVal;` |
|    2494 |  783 | `			prefix = 0;` |
|       - |  784 | `		}` |
|       - |  785 | `        /* Zero padding is a RIGHT-aligned idea: it fills between the sign and the` |
|       - |  786 | `         * first digit. Left-aligned, php pads on the far side like any other pad` |
|       - |  787 | `         * character (append2n hands the '0' straight to appendstring's ALIGN_LEFT` |
|       - |  788 | `         * arm), so "%-08x" of 5 is "50000000". */` |
|   12592 |  789 | `        if( flag_zeropad && !flag_leftjustify ){` |
|    2314 |  790 | `          cLeadSign = (char)prefix;` |
|    2314 |  791 | `          prefix = 0;` |
|    1155 |  792 | `        }` |
|   12592 |  793 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|       - |  794 | `        {` |
|       - |  795 | `          const char *cset;` |
|       - |  796 | `          sxu64 base;` |
|   12592 |  797 | `          cset = pInfo->charset;` |
|   12592 |  798 | `          base = (sxu64)pInfo->base;` |
|    6253 |  799 | `          do{                                           /* Convert to ascii */` |
|   28009 |  800 | `            *(--zBuf) = cset[uVal%base];` |
|   28009 |  801 | `            uVal = uVal/base;` |
|   28009 |  802 | `          }while( uVal>0 );` |
|       - |  803 | `        }` |
|   12592 |  804 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|       - |  805 | `        /* No zero fill here: a radix conversion has no precision to fill to, and` |
|       - |  806 | `         * the '0' pad is the output block's job now (see cLeadSign). */` |
|   12592 |  807 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|   12592 |  808 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|       - |  809 | `          char *pre, x;` |
|     ! 0 |  810 | `          pre = pInfo->prefix;` |
|     ! 0 |  811 | `          if( *zBuf!=pre[0] ){` |
|     ! 0 |  812 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|     ! 0 |  813 | `          }` |
|     ! 0 |  814 | `        }` |
|   12592 |  815 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   12592 |  816 | `		if( bDropDigits ){` |
|       9 |  817 | `			length = 0;` |
|       4 |  818 | `		}` |
|   12592 |  819 | `		break;` |
|       - |  820 | `		}` |
|     195 |  821 | `		case PH7_FMT_FLOAT:` |
|       - |  822 | `		case PH7_FMT_EXP:` |
|       - |  823 | `		case PH7_FMT_GENERIC:{` |
|       - |  824 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - |  825 | `		double realvalue;` |
|       - |  826 | `		char zFmt[8];` |
|       - |  827 | `		int nOut, nFmt;` |
|     393 |  828 | `		pArg = NEXT_ARG;` |
|     393 |  829 | `		if( pArg == 0 ){` |
|     ! 0 |  830 | `			realvalue = 0;` |
|     ! 0 |  831 | `		}else{` |
|     393 |  832 | `			realvalue = ph7_value_to_double(pArg);` |
|       - |  833 | `		}` |
|       - |  834 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|       - |  835 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|     393 |  836 | `		if( PH7_IS_NAN(realvalue) ){` |
|      23 |  837 | `			zBuf = "NaN";` |
|      23 |  838 | `			length = 3;` |
|      23 |  839 | `			width = 0;` |
|      23 |  840 | `			break;` |
|       - |  841 | `		}` |
|     371 |  842 | `		if( PH7_IS_INF(realvalue) ){` |
|      37 |  843 | `			if( realvalue < 0.0 ){` |
|      15 |  844 | `				zBuf = "-INF";` |
|      15 |  845 | `				length = 4;` |
|       8 |  846 | `			}else{` |
|      23 |  847 | `				zBuf = "INF";` |
|      23 |  848 | `				length = 3;` |
|       - |  849 | `			}` |
|      37 |  850 | `			width = 0;` |
|      37 |  851 | `			break;` |
|       - |  852 | `		}` |
|     335 |  853 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     335 |  854 | `		if( precision > 53 ){` |
|       - |  855 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|       - |  856 | `			 * (message prefixed with the active function's name, like` |
|       - |  857 | `			 * php_error_docref). */` |
|       - |  858 | `			char zMsg[160];` |
|       4 |  859 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  860 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|       2 |  861 | `				&pCtx->pFunc->sName,precision,53);` |
|       3 |  862 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|       3 |  863 | `			precision = 53;` |
|       1 |  864 | `		}` |
|       - |  865 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|       - |  866 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|     335 |  867 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|       9 |  868 | `			realvalue = 0.0;` |
|       4 |  869 | `		}` |
|       - |  870 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|       - |  871 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|       - |  872 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|       - |  873 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|       - |  874 | `		 * expansion), then post-process into php's exact shapes below. */` |
|     335 |  875 | `		nFmt = 0;` |
|     335 |  876 | `		zFmt[nFmt++] = '%';` |
|     335 |  877 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|       - |  878 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|       - |  879 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|     335 |  880 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|     335 |  881 | `		zFmt[nFmt++] = '.';` |
|     335 |  882 | `		zFmt[nFmt++] = '*';` |
|     549 |  883 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     115 |  884 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     198 |  885 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|     335 |  886 | `		zFmt[nFmt] = 0;` |
|     335 |  887 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|     335 |  888 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|       - |  889 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|       - |  890 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|     ! 0 |  891 | `			nOut = (int)SyStrlen(zWorker);` |
|     ! 0 |  892 | `		}` |
|     335 |  893 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|     335 |  894 | `		zBuf = zWorker;` |
|     335 |  895 | `		length = nOut;` |
|       - |  896 | `		/* The zero padding goes between the sign snprintf wrote and the first` |
|       - |  897 | `		 * digit, so hand the sign to the output block and leave the rest here. */` |
|     332 |  898 | `		if( flag_zeropad && !flag_leftjustify` |
|      23 |  899 | `		 && (zWorker[0]=='-' \|\| zWorker[0]=='+') ){` |
|      11 |  900 | `			cLeadSign = zWorker[0];` |
|      11 |  901 | `			zBuf++;` |
|      11 |  902 | `			length--;` |
|       5 |  903 | `		}` |
|       - |  904 | `#else` |
|       - |  905 | `         zBuf = " ";` |
|       - |  906 | `		 length = (int)sizeof(char);` |
|       - |  907 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     335 |  908 | `		 break;` |
|       - |  909 | `							 }` |
|     ! 0 |  910 | `		default:` |
|       - |  911 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|       - |  912 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|       - |  913 | `			 * no-op that emits nothing. */` |
|     ! 0 |  914 | `			length = 0;` |
|     ! 0 |  915 | `			break;` |
|       - |  916 | `		}` |
|       - |  917 | `		 /*` |
|       - |  918 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|       - |  919 | `		 ** "length" characters long.The field width is "width".Do` |
|       - |  920 | `		 ** the output.` |
|       - |  921 | `		 */` |
|   71063 |  922 | `    if( cLeadSign ){` |
|       - |  923 | `      /* php writes the sign ahead of a '0' pad run; it fills one byte of the` |
|       - |  924 | `       * field, so the padding below has that much less to do. */` |
|      37 |  925 | `      rc = xConsumer(pCtx,&cLeadSign,1,pUserData);` |
|      37 |  926 | `      if( rc != SXRET_OK ){` |
|     ! 0 |  927 | `        rcRet = SXERR_ABORT;` |
|     ! 0 |  928 | `        goto Done;` |
|       - |  929 | `      }` |
|      37 |  930 | `      width--;` |
|      18 |  931 | `    }` |
|   71063 |  932 | `    if( width > length ){` |
|       - |  933 | `      /* Fill the pad buffer with THIS specifier's pad character. */` |
| 1392662 |  934 | `      for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = cPad; }` |
|   13490 |  935 | `    }` |
|   71063 |  936 | `    if( !flag_leftjustify ){` |
|       - |  937 | `      register int nspace;` |
|   42982 |  938 | `      nspace = width-length;` |
|   42982 |  939 | `      if( nspace>0 ){` |
|    3501 |  940 | `        while( nspace>=etSPACESIZE ){` |
|    2103 |  941 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    2103 |  942 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  943 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  944 | `				goto Done;` |
|       - |  945 | `			}` |
|    2103 |  946 | `			nspace -= etSPACESIZE;` |
|       1 |  947 | `        }` |
|    1399 |  948 | `        if( nspace>0 ){` |
|    1399 |  949 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    1399 |  950 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  951 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  952 | `				goto Done;` |
|       - |  953 | `			}` |
|     692 |  954 | `		}` |
|     692 |  955 | `      }` |
|   21295 |  956 | `    }` |
|   71063 |  957 | `    if( length>0 ){` |
|   70227 |  958 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   70227 |  959 | `		if( rc != SXRET_OK ){` |
|       3 |  960 | `		  rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       3 |  961 | `		  goto Done;` |
|       - |  962 | `		}` |
|   34757 |  963 | `    }` |
|   71061 |  964 | `    if( flag_leftjustify ){` |
|       - |  965 | `      register int nspace;` |
|   28086 |  966 | `      nspace = width-length;` |
|   28086 |  967 | `      if( nspace>0 ){` |
|   26442 |  968 | `        while( nspace>=etSPACESIZE ){` |
|     526 |  969 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|     526 |  970 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  971 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  972 | `				goto Done;` |
|       - |  973 | `			}` |
|     526 |  974 | `			nspace -= etSPACESIZE;` |
|       2 |  975 | `        }` |
|   25918 |  976 | `        if( nspace>0 ){` |
|   25918 |  977 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|   25918 |  978 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  979 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  980 | `				goto Done;` |
|       - |  981 | `			}` |
|   12798 |  982 | `		}` |
|   12798 |  983 | `      }` |
|   13881 |  984 | `    }` |
|       5 |  985 | ` }/* for(;;) */` |
|   26409 |  986 | `	if( pThrowArg ){` |
|       - |  987 | `		/* The format ran to completion and its output is out; raise php's Error` |
|       - |  988 | ``		 * now. `printf("A[%s]B", new P())` prints "A[]B" and THEN throws, while`` |
|       - |  989 | `		 * sprintf()'s finished result is simply discarded by the unwind. */` |
|      19 |  990 | `		PH7_MemObjRelease(&sScratch);` |
|      19 |  991 | `		return PH7_MemObjToStringUV(pThrowArg);` |
|       - |  992 | `	}` |
|   13363 |  993 | `Done:` |
|       - |  994 | `	/* Single exit: the scratch copy holds a reference on an array/instance` |
|       - |  995 | `	 * argument it was loaded from, so every way out releases it. */` |
|   26395 |  996 | `	PH7_MemObjRelease(&sScratch);` |
|   26395 |  997 | `	return rcRet;` |
|   13039 |  998 | `}` |
|       - |  999 | `/*` |
|       - | 1000 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|       - | 1001 | ` */` |
|   14402 | 1002 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       5 | 1003 | `{` |
|       - | 1004 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|       - | 1005 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|       - | 1006 | `	 * non-OK rc also stops the format loop. */` |
|   14407 | 1007 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|   14407 | 1008 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|   14407 | 1009 | `	return *pRc;` |
|       5 | 1010 | `}` |
|       - | 1011 | `/*` |
|       - | 1012 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|       - | 1013 | ` *  Return a formatted string.` |
|       - | 1014 | ` * Parameters` |
|       - | 1015 | ` *  $format` |
|       - | 1016 | ` *    The format string (see block comment above)` |
|       - | 1017 | ` * Return` |
|       - | 1018 | ` *  A string produced according to the formatting string format.` |
|       - | 1019 | ` */` |
|    3178 | 1020 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1021 | `{` |
|       - | 1022 | `	sxi32 rcFmt;` |
|       - | 1023 | `	const char *zFormat;` |
|    3183 | 1024 | `	sxi32 rc = SXRET_OK;` |
|       - | 1025 | `	int nLen;` |
|    3183 | 1026 | `	if( nArg < 1 ){` |
|       - | 1027 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 1028 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1029 | `		return PH7_OK;` |
|       - | 1030 | `	}` |
|       - | 1031 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    3183 | 1032 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    3183 | 1033 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1034 | `		return rc;` |
|       - | 1035 | `	}` |
|       - | 1036 | `	/* Extract the string format (scalars/null coerce). */` |
|    3183 | 1037 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    3183 | 1038 | `	if( nLen < 1 ){` |
|       - | 1039 | `		/* Empty string */` |
|     ! 0 | 1040 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1041 | `		return PH7_OK;` |
|       - | 1042 | `	}` |
|       - | 1043 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|       - | 1044 | `	 * output; propagate the throw status verbatim. */` |
|    3183 | 1045 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    3183 | 1046 | `	if( rc != PH7_OK ){` |
|      41 | 1047 | `		return rc;` |
|       - | 1048 | `	}` |
|       - | 1049 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    3143 | 1050 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    3143 | 1051 | `	if( rc != PH7_OK ){` |
|      31 | 1052 | `		return rc;` |
|       - | 1053 | `	}` |
|       - | 1054 | `	/* Seed the result with the empty string: a format whose every conversion` |
|       - | 1055 | `	 * substitutes NOTHING ("%s" of "", false or null) never calls the consumer at` |
|       - | 1056 | `	 * all, and an untouched return value is NULL — so sprintf("%s","") answered` |
|       - | 1057 | `	 * NULL where php answers "". */` |
|    3113 | 1058 | `	ph7_result_string(pCtx,"",0);` |
|       - | 1059 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    3113 | 1060 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    3113 | 1061 | `	if( rc != SXRET_OK ){` |
|       - | 1062 | `		/* The result append ran out of memory: raise a fatal rather than` |
|       - | 1063 | `		 * returning a silently-truncated string. */` |
|     ! 0 | 1064 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1065 | `	}` |
|       - | 1066 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1067 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1068 | `	 * that), so report the throw last. */` |
|    3113 | 1069 | `	if( rcFmt != SXRET_OK ){` |
|      12 | 1070 | `		pCtx->nThrowRc = rcFmt;` |
|      12 | 1071 | `		return rcFmt;` |
|       - | 1072 | `	}` |
|    3103 | 1073 | `	return PH7_OK;` |
|    1591 | 1074 | `}` |
|       - | 1075 | `/*` |
|       - | 1076 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|       - | 1077 | ` */` |
|  159539 | 1078 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       5 | 1079 | `{` |
|  159544 | 1080 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|       - | 1081 | `	/* Call the VM output consumer directly */` |
|  159544 | 1082 | `	ph7_context_output(pCtx,zInput,nLen);` |
|       - | 1083 | `	/* Increment counter */` |
|  159544 | 1084 | `	*pCounter += nLen;` |
|  159544 | 1085 | `	return PH7_OK;` |
|       5 | 1086 | `}` |
|       - | 1087 | `/*` |
|       - | 1088 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|       - | 1089 | ` *  Output a formatted string.` |
|       - | 1090 | ` * Parameters` |
|       - | 1091 | ` *  $format` |
|       - | 1092 | ` *   See sprintf() for a description of format.` |
|       - | 1093 | ` * Return` |
|       - | 1094 | ` *  The length of the outputted string.` |
|       - | 1095 | ` */` |
|   23244 | 1096 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1097 | `{` |
|       - | 1098 | `	sxi32 rcFmt;` |
|   23249 | 1099 | `	ph7_int64 nCounter = 0;` |
|       - | 1100 | `	const char *zFormat;` |
|       - | 1101 | `	int nLen;` |
|   23249 | 1102 | `	if( nArg < 1 ){` |
|       - | 1103 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 1104 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1105 | `		return PH7_OK;` |
|       - | 1106 | `	}` |
|       - | 1107 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|       - | 1108 | `	{` |
|   23249 | 1109 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   23249 | 1110 | `		if( rcf != PH7_OK ){` |
|     ! 0 | 1111 | `			return rcf;` |
|       - | 1112 | `		}` |
|       - | 1113 | `	}` |
|       - | 1114 | `	/* Extract the string format (scalars/null coerce). */` |
|   23249 | 1115 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   23249 | 1116 | `	if( nLen < 1 ){` |
|       - | 1117 | `		/* Empty string */` |
|     ! 0 | 1118 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1119 | `		return PH7_OK;` |
|       - | 1120 | `	}` |
|       - | 1121 | `	{` |
|       - | 1122 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|       - | 1123 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|   23249 | 1124 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   23249 | 1125 | `		if( rcv != PH7_OK ){` |
|       3 | 1126 | `			return rcv;` |
|       - | 1127 | `		}` |
|       - | 1128 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|       - | 1129 | `		 * before any output; propagate the throw status verbatim. */` |
|   23247 | 1130 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   23247 | 1131 | `		if( rcv != PH7_OK ){` |
|     ! 0 | 1132 | `			return rcv;` |
|       - | 1133 | `		}` |
|       - | 1134 | `	}` |
|       - | 1135 | `	/* Format the string */` |
|   23247 | 1136 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|       - | 1137 | `	/* Return the length of the outputted string */` |
|   23247 | 1138 | `	ph7_result_int64(pCtx,nCounter);` |
|       - | 1139 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1140 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1141 | `	 * that), so report the throw last. */` |
|   23247 | 1142 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1143 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1144 | `		return rcFmt;` |
|       - | 1145 | `	}` |
|   23245 | 1146 | `	return PH7_OK;` |
|   11460 | 1147 | `}` |
|       - | 1148 | `/*` |
|       - | 1149 | ` * int vprintf(string $format,array $args)` |
|       - | 1150 | ` *  Output a formatted string.` |
|       - | 1151 | ` * Parameters` |
|       - | 1152 | ` *  $format` |
|       - | 1153 | ` *   See sprintf() for a description of format.` |
|       - | 1154 | ` * Return` |
|       - | 1155 | ` *  The length of the outputted string.` |
|       - | 1156 | ` */` |
|       8 | 1157 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1158 | `{` |
|       - | 1159 | `	sxi32 rcFmt;` |
|      11 | 1160 | `	ph7_int64 nCounter = 0;` |
|       - | 1161 | `	const char *zFormat;` |
|       - | 1162 | `	ph7_hashmap *pMap;` |
|       - | 1163 | `	SySet sArg;` |
|       - | 1164 | `	int nLen,n;` |
|      11 | 1165 | `	if( nArg < 2 ){` |
|       - | 1166 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 1167 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1168 | `		return PH7_OK;` |
|       - | 1169 | `	}` |
|       - | 1170 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      11 | 1171 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      11 | 1172 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1173 | `		return rcFmt;` |
|       - | 1174 | `	}` |
|      11 | 1175 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1176 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - | 1177 | `		char zBuf[64];` |
|     ! 0 | 1178 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1179 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 1180 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 1181 | `	}` |
|       - | 1182 | `	/* Extract the string format (scalars/null coerce). */` |
|      11 | 1183 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      11 | 1184 | `	if( nLen < 1 ){` |
|       - | 1185 | `		/* Empty string */` |
|     ! 0 | 1186 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1187 | `		return PH7_OK;` |
|       - | 1188 | `	}` |
|       - | 1189 | `	/* Point to the hashmap */` |
|      11 | 1190 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1191 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - | 1192 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|       - | 1193 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|       - | 1194 | `	 * rather than the missing specifier. */` |
|      11 | 1195 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      11 | 1196 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1197 | `		return rcFmt;` |
|       - | 1198 | `	}` |
|       - | 1199 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - | 1200 | `	 * any output; propagate the throw status verbatim. */` |
|      11 | 1201 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      11 | 1202 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1203 | `		return rcFmt;` |
|       - | 1204 | `	}` |
|       - | 1205 | `	/* Extract arguments from the hashmap */` |
|      11 | 1206 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - | 1207 | `	/* Format the string */` |
|      11 | 1208 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|       - | 1209 | `	/* Release the container */` |
|      11 | 1210 | `	SySetRelease(&sArg);` |
|       - | 1211 | `	/* Return the length of the outputted string */` |
|      11 | 1212 | `	ph7_result_int64(pCtx,nCounter);` |
|       - | 1213 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1214 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1215 | `	 * that), so report the throw last. */` |
|      11 | 1216 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1217 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1218 | `		return rcFmt;` |
|       - | 1219 | `	}` |
|       8 | 1220 | `	return PH7_OK;` |
|       7 | 1221 | `}` |
|       - | 1222 | `/*` |
|       - | 1223 | ` * int vsprintf(string $format,array $args)` |
|       - | 1224 | ` *  Output a formatted string.` |
|       - | 1225 | ` * Parameters` |
|       - | 1226 | ` *  $format` |
|       - | 1227 | ` *   See sprintf() for a description of format.` |
|       - | 1228 | ` * Return` |
|       - | 1229 | ` *  A string produced according to the formatting string format.` |
|       - | 1230 | ` */` |
|      28 | 1231 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1232 | `{` |
|       - | 1233 | `	sxi32 rcFmt;` |
|       - | 1234 | `	const char *zFormat;` |
|       - | 1235 | `	ph7_hashmap *pMap;` |
|       - | 1236 | `	SySet sArg;` |
|      32 | 1237 | `	sxi32 rc = SXRET_OK;` |
|       - | 1238 | `	int nLen,n;` |
|      32 | 1239 | `	if( nArg < 2 ){` |
|       - | 1240 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 1241 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1242 | `		return PH7_OK;` |
|       - | 1243 | `	}` |
|       - | 1244 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      32 | 1245 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      32 | 1246 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1247 | `		return rc;` |
|       - | 1248 | `	}` |
|      32 | 1249 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1250 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - | 1251 | `		char zBuf[64];` |
|     ! 0 | 1252 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1253 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 1254 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 1255 | `	}` |
|       - | 1256 | `	/* Extract the string format (scalars/null coerce). */` |
|      32 | 1257 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      32 | 1258 | `	if( nLen < 1 ){` |
|       - | 1259 | `		/* Empty string */` |
|     ! 0 | 1260 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1261 | `		return PH7_OK;` |
|       - | 1262 | `	}` |
|       - | 1263 | `	/* Point to hashmap */` |
|      32 | 1264 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1265 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - | 1266 | `	 * php runs this BEFORE validating the specifiers. */` |
|      32 | 1267 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      32 | 1268 | `	if( rcFmt != PH7_OK ){` |
|       7 | 1269 | `		return rcFmt;` |
|       - | 1270 | `	}` |
|       - | 1271 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - | 1272 | `	 * any output; propagate the throw status verbatim. */` |
|      26 | 1273 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      26 | 1274 | `	if( rcFmt != PH7_OK ){` |
|       3 | 1275 | `		return rcFmt;` |
|       - | 1276 | `	}` |
|       - | 1277 | `	/* Extract arguments from the hashmap */` |
|      24 | 1278 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - | 1279 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|       - | 1280 | `	/* Empty-result seed — see PH7_builtin_sprintf. */` |
|      24 | 1281 | `	ph7_result_string(pCtx,"",0);` |
|      24 | 1282 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|       - | 1283 | `	/* Release the container */` |
|      24 | 1284 | `	SySetRelease(&sArg);` |
|      24 | 1285 | `	if( rc != SXRET_OK ){` |
|       - | 1286 | `		/* The result append ran out of memory: raise a fatal. */` |
|     ! 0 | 1287 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1290 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1291 | `	 * that), so report the throw last. */` |
|      24 | 1292 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1293 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1294 | `		return rcFmt;` |
|       - | 1295 | `	}` |
|      21 | 1296 | `	return PH7_OK;` |
|      18 | 1297 | `}` |
|       - | 1298 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - | 1299 |  |
