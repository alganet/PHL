# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 548/620 lines (88.39%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|      - |    8 | `                      * rounded shortest-representation output) */` |
|      - |    9 | `/*` |
|      - |   10 | ` * Section:` |
|      - |   11 | ` *    printf-style format engine and the sprintf/printf function family.` |
|      - |   12 | ` * Status:` |
|      - |   13 | ` *    Stable.` |
|      - |   14 | ` */` |
|      - |   15 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   16 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |   17 | `#endif` |
|      - |   18 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - |   19 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - |   20 | `/*` |
|      - |   21 | `** Conversion types fall into various categories as defined by the` |
|      - |   22 | `** following enumeration.` |
|      - |   23 | `*/` |
|      - |   24 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - |   25 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - |   26 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - |   27 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - |   28 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - |   29 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - |   30 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - |   31 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - |   32 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - |   33 |  |
|      - |   34 | `/*` |
|      - |   35 | `** Allowed values for ph7_fmt_info.flags` |
|      - |   36 | `*/` |
|      - |   37 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - |   38 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - |   39 | `/*` |
|      - |   40 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - |   41 | `** by an instance of the following structure` |
|      - |   42 | `*/` |
|      - |   43 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - |   44 | `struct ph7_fmt_info` |
|      - |   45 | `{` |
|      - |   46 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - |   47 | `  sxu8 base;     /* The base for radix conversion */` |
|      - |   48 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - |   49 | `  sxu8 type;     /* Conversion paradigm */` |
|      - |   50 | `  char *charset; /* The character set for conversion */` |
|      - |   51 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - |   52 | `};` |
|      - |   53 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - |   54 | ` * the default float->string cast needs it even when this whole formatting` |
|      - |   55 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - |   56 | `/*` |
|      - |   57 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - |   58 | ` * used conversion types first.` |
|      - |   59 | ` */` |
|      - |   60 | `static const ph7_fmt_info aFmt[] = {` |
|      - |   61 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - |   62 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - |   63 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - |   64 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - |   65 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - |   66 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - |   67 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - |   68 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - |   69 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - |   70 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - |   71 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - |   72 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - |   73 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - |   74 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - |   75 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - |   76 | `   * formats in the C locale, so they behave identically. */` |
|      - |   77 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - |   78 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - |   79 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - |   80 | `};` |
|      - |   81 | `/*` |
|      - |   82 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - |   83 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - |   84 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - |   85 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - |   86 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - |   87 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - |   88 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - |   89 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - |   90 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - |   91 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - |   92 | ` * "all valid".)` |
|      - |   93 | ` */` |
|   2230 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|      5 |   95 | `{` |
|   2235 |   96 | `	const char *zEnd = &zIn[nByte];` |
|      - |   97 | `	int c,idx;` |
|  27723 |   98 | `	while( zIn < zEnd ){` |
|  25529 |   99 | `		if( zIn[0] != '%' ){` |
|  19579 |  100 | `			zIn++;` |
|  19579 |  101 | `			continue;` |
|      - |  102 | `		}` |
|   5955 |  103 | `		zIn++; /* jump the percent sign */` |
|      - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - |  106 | `		 * unknown specifier, matching php. */` |
|   8787 |  107 | `		while( zIn < zEnd ){` |
|   8775 |  108 | `			c = zIn[0];` |
|   8775 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   2789 |  110 | `				zIn++;` |
|   2789 |  111 | `				continue;` |
|      - |  112 | `			}` |
|   5991 |  113 | `			if( c=='\'' ){` |
|     49 |  114 | `				zIn++;` |
|     49 |  115 | `				if( zIn < zEnd ){` |
|     49 |  116 | `					zIn++; /* the custom pad character */` |
|     24 |  117 | `				}` |
|     49 |  118 | `				continue;` |
|      - |  119 | `			}` |
|   5943 |  120 | `			break;` |
|    ! 0 |  121 | `		}` |
|      - |  122 | `		/* field width */` |
|  10667 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   4717 |  124 | `			zIn++;` |
|      5 |  125 | `		}` |
|      - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   5955 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     50 |  129 | `			zIn++;` |
|     52 |  130 | `			while( zIn < zEnd ){` |
|     50 |  131 | `				c = zIn[0];` |
|     50 |  132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 |  133 | `					zIn++;` |
|    ! 0 |  134 | `					continue;` |
|      - |  135 | `				}` |
|     50 |  136 | `				if( c=='\'' ){` |
|      3 |  137 | `					zIn++;` |
|      3 |  138 | `					if( zIn < zEnd ){` |
|      3 |  139 | `						zIn++;` |
|      1 |  140 | `					}` |
|      3 |  141 | `					continue;` |
|      - |  142 | `				}` |
|     48 |  143 | `				break;` |
|    ! 0 |  144 | `			}` |
|     58 |  145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 |  146 | `				zIn++;` |
|      1 |  147 | `			}` |
|     24 |  148 | `		}` |
|      - |  149 | `		/* precision */` |
|   5955 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    146 |  151 | `			zIn++;` |
|    300 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    156 |  153 | `				zIn++;` |
|      2 |  154 | `			}` |
|     72 |  155 | `		}` |
|      - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   5955 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 |  158 | `			zIn++;` |
|      5 |  159 | `		}` |
|   5955 |  160 | `		if( zIn >= zEnd ){` |
|      - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|      - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|     17 |  163 | `			*pbDangling = TRUE;` |
|     17 |  164 | `			return FALSE;` |
|      - |  165 | `		}` |
|   5939 |  166 | `		c = zIn[0];` |
|   5939 |  167 | `		zIn++; /* jump the conversion specifier */` |
|  13697 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|  13677 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   5919 |  170 | `				break;` |
|      - |  171 | `			}` |
|   3884 |  172 | `		}` |
|   5939 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     21 |  174 | `			*pBad = c; /* unknown specifier */` |
|     21 |  175 | `			return TRUE;` |
|      - |  176 | `		}` |
|      5 |  177 | `	}` |
|   2199 |  178 | `	return FALSE;` |
|   1120 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - |  186 | ` * Returns PH7_OK when the format is valid.` |
|      - |  187 | ` */` |
|   2230 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      5 |  189 | `{` |
|   2235 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   2235 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|     31 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     10 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - |  194 | `	}` |
|   2215 |  195 | `	if( bDangling ){` |
|     17 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  197 | `			"Missing format specifier at end of string");` |
|      - |  198 | `	}` |
|   2199 |  199 | `	return PH7_OK;` |
|   1120 |  200 | `}` |
|      - |  201 | `/*` |
|      - |  202 | ` * Read a run of decimal digits, saturating at PH7_FMT_NUM_CAP rather than` |
|      - |  203 | `` * wrapping: `%99999999999999999999d` must be REPORTED, and a signed overflow on`` |
|      - |  204 | ` * the way to reporting it is undefined behaviour (it used to make the width come` |
|      - |  205 | ` * out negative, or a positional index come out as an ordinary sequential one).` |
|      - |  206 | ` */` |
|      - |  207 | `#define PH7_FMT_NUM_CAP 2147483647` |
|  12322 |  208 | `static int FormatScanNumber(const char **pzIn,const char *zEnd)` |
|      5 |  209 | `{` |
|  12327 |  210 | `	const char *zIn = *pzIn;` |
|  12327 |  211 | `	int v = 0;` |
|  22241 |  212 | `	while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   9919 |  213 | `		int d = zIn[0]-'0';` |
|      - |  214 | `		/* Tested BEFORE the multiply: a signed overflow is undefined, so a guard` |
|      - |  215 | `		 * that inspects the wrapped result is one an optimiser may delete — and` |
|      - |  216 | ``		 * did, which is how `%2147483648d` slipped past the range check below. */`` |
|   9919 |  217 | `		if( v > (PH7_FMT_NUM_CAP - d)/10 ){` |
|     29 |  218 | `			v = PH7_FMT_NUM_CAP;` |
|     15 |  219 | `		}else{` |
|   9891 |  220 | `			v = v*10 + d;` |
|      - |  221 | `		}` |
|   9919 |  222 | `		zIn++;` |
|      5 |  223 | `	}` |
|  12327 |  224 | `	*pzIn = zIn;` |
|  12327 |  225 | `	return v;` |
|      5 |  226 | `}` |
|      - |  227 | `/*` |
|      - |  228 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - |  229 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - |  230 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - |  231 | ` *` |
|      - |  232 | ` * php also refuses a specifier over its own SHAPE before it looks at how many` |
|      - |  233 | ` * values it was given: the three numbers it can carry are bounded, and a` |
|      - |  234 | ` * custom-pad flag the string ends on is named in its own right. *pzBad receives` |
|      - |  235 | ` * php's message for the FIRST such specifier — the scan returns there, since` |
|      - |  236 | ` * php's is a single left-to-right pass and the count it would have produced is` |
|      - |  237 | ` * moot once one of these is raised.` |
|      - |  238 | ` */` |
|      - |  239 | `/* The bound each message quotes is PH7_FMT_NUM_CAP above; php spells it out, so` |
|      - |  240 | ` * these do too (its two shapes differ: the positional one names the open` |
|      - |  241 | ` * interval, the other two the closed one). */` |
|      - |  242 | `#define PH7_FMT_BAD_ARGNUM \` |
|      - |  243 | `	"Argument number specifier must be greater than zero and less than 2147483647"` |
|      - |  244 | `#define PH7_FMT_BAD_WIDTH     "Width must be between 0 and 2147483647"` |
|      - |  245 | `#define PH7_FMT_BAD_PRECISION "Precision must be between 0 and 2147483647"` |
|      - |  246 | `#define PH7_FMT_BAD_PAD       "Missing padding character"` |
|   2282 |  247 | `static int FormatRequiredArgs(const char *zIn,int nByte,const char **pzBad)` |
|      5 |  248 | `{` |
|   2287 |  249 | `	const char *zEnd = &zIn[nByte];` |
|   2287 |  250 | `	int c,seq = 0,maxpos = 0;` |
|  27875 |  251 | `	while( zIn < zEnd ){` |
|  25639 |  252 | `		int numVal = 0,pos = 0;` |
|  25639 |  253 | `		if( zIn[0] != '%' ){` |
|  19619 |  254 | `			zIn++;` |
|  19619 |  255 | `			continue;` |
|      - |  256 | `		}` |
|   6025 |  257 | `		zIn++; /* jump the percent sign */` |
|      - |  258 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   8861 |  259 | `		while( zIn < zEnd ){` |
|   8843 |  260 | `			c = zIn[0];` |
|   8843 |  261 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|   6055 |  262 | `			if( c=='\'' ){` |
|     53 |  263 | `				zIn++;` |
|     53 |  264 | `				if( zIn >= zEnd ){` |
|      - |  265 | `					/* A custom-pad flag the string ends on: php names THAT, not the` |
|      - |  266 | `					 * specifier it also lacks, and it does so before counting. */` |
|      5 |  267 | `					*pzBad = PH7_FMT_BAD_PAD;` |
|      5 |  268 | `					return seq;` |
|      - |  269 | `				}` |
|     49 |  270 | `				zIn++;` |
|     49 |  271 | `				continue;` |
|      - |  272 | `			}` |
|   6003 |  273 | `			break;` |
|    ! 0 |  274 | `		}` |
|      - |  275 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   6021 |  276 | `		numVal = FormatScanNumber(&zIn,zEnd);` |
|   6021 |  277 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     68 |  278 | `			pos = numVal;` |
|      - |  279 | ``			/* php: `0 < N < 2147483647`, so `%0$s` and `%2147483647$s` are both the`` |
|      - |  280 | `			 * ValueError — the second used to overflow the required-count report to` |
|      - |  281 | `			 * a NEGATIVE number, and anything past it fell back to sequential. */` |
|     68 |  282 | `			if( pos < 1 \|\| pos >= PH7_FMT_NUM_CAP ){` |
|      9 |  283 | `				*pzBad = PH7_FMT_BAD_ARGNUM;` |
|      9 |  284 | `				return seq;` |
|      - |  285 | `			}` |
|     60 |  286 | `			zIn++;` |
|      - |  287 | `			/* flags then width may follow the positional marker */` |
|     62 |  288 | `			while( zIn < zEnd ){` |
|     60 |  289 | `				c = zIn[0];` |
|     60 |  290 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     60 |  291 | `				if( c=='\'' ){` |
|      5 |  292 | `					zIn++;` |
|      5 |  293 | `					if( zIn >= zEnd ){` |
|      3 |  294 | `						*pzBad = PH7_FMT_BAD_PAD;` |
|      3 |  295 | `						return seq;` |
|      - |  296 | `					}` |
|      3 |  297 | `					zIn++;` |
|      3 |  298 | `					continue;` |
|      - |  299 | `				}` |
|     56 |  300 | `				break;` |
|    ! 0 |  301 | `			}` |
|     58 |  302 | `			numVal = FormatScanNumber(&zIn,zEnd);` |
|     28 |  303 | `		}` |
|   6011 |  304 | `		if( numVal >= PH7_FMT_NUM_CAP ){` |
|      7 |  305 | `			*pzBad = PH7_FMT_BAD_WIDTH;` |
|      7 |  306 | `			return seq;` |
|      - |  307 | `		}` |
|      - |  308 | `		/* precision */` |
|   6005 |  309 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    150 |  310 | `			zIn++;` |
|    150 |  311 | `			if( FormatScanNumber(&zIn,zEnd) >= PH7_FMT_NUM_CAP ){` |
|      5 |  312 | `				*pzBad = PH7_FMT_BAD_PRECISION;` |
|      5 |  313 | `				return seq;` |
|      - |  314 | `			}` |
|     72 |  315 | `		}` |
|      - |  316 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   6001 |  317 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|   6001 |  318 | `		if( zIn >= zEnd ){` |
|      - |  319 | `			/* A dangling '%' still COUNTS as needing a value: php reports` |
|      - |  320 | `			 * sprintf("%") as "2 arguments are required, 1 given" and only` |
|      - |  321 | `			 * raises the missing-specifier ValueError once the count is met. */` |
|     23 |  322 | `			if( pos > 0 ){` |
|      3 |  323 | `				if( pos > maxpos ){ maxpos = pos; }` |
|      2 |  324 | `			}else{` |
|     21 |  325 | `				seq++;` |
|      - |  326 | `			}` |
|     23 |  327 | `			break;` |
|      - |  328 | `		}` |
|   5979 |  329 | `		c = zIn[0];` |
|   5979 |  330 | `		zIn++; /* jump the conversion specifier */` |
|   5979 |  331 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   5969 |  332 | `		if( pos > 0 ){` |
|     56 |  333 | `			if( pos > maxpos ){ maxpos = pos; }` |
|     29 |  334 | `		}else{` |
|   5915 |  335 | `			seq++;` |
|      - |  336 | `		}` |
|      5 |  337 | `	}` |
|   2263 |  338 | `	return seq > maxpos ? seq : maxpos;` |
|   1146 |  339 | `}` |
|      - |  340 | `/*` |
|      - |  341 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - |  342 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - |  343 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - |  344 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - |  345 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - |  346 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - |  347 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - |  348 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - |  349 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - |  350 | ` * when enough.` |
|      - |  351 | ` */` |
|   2282 |  352 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      5 |  353 | `{` |
|   2287 |  354 | `	const char *zBad = 0;` |
|   2287 |  355 | `	int required = FormatRequiredArgs(zFormat,nByte,&zBad);` |
|   2287 |  356 | `	if( zBad ){` |
|      - |  357 | `		/* php refuses a specifier's own shape before it counts the values, so` |
|      - |  358 | ``		 * `sprintf("%2$s%0$s","a")` and `sprintf("%'","a")` are the ValueError and`` |
|      - |  359 | `		 * not the (also true) ArgumentCountError. */` |
|     25 |  360 | `		return PH7_VmThrowException(pCtx,"ValueError","%s",zBad);` |
|      - |  361 | `	}` |
|   2263 |  362 | `	if( nValues < required ){` |
|     29 |  363 | `		if( bVararg ){` |
|     13 |  364 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 |  365 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - |  366 | `		}` |
|     31 |  367 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     10 |  368 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - |  369 | `	}` |
|   2235 |  370 | `	return PH7_OK;` |
|   1146 |  371 | `}` |
|      - |  372 | `/*` |
|      - |  373 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - |  374 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - |  375 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - |  376 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - |  377 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - |  378 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - |  379 | ` */` |
|      - |  380 | `/*` |
|      - |  381 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - |  382 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - |  383 | ` */` |
|     28 |  384 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      2 |  385 | `{` |
|     30 |  386 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - |  387 | `		char zBuf[64];` |
|      4 |  388 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  389 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 |  390 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - |  391 | `	}` |
|     28 |  392 | `	return PH7_OK;` |
|     16 |  393 | `}` |
|   2282 |  394 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      5 |  395 | `{` |
|   2287 |  396 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - |  397 | `		char zBuf[64];` |
|    ! 0 |  398 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  399 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 |  400 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - |  401 | `	}` |
|   2287 |  402 | `	return PH7_OK;` |
|   1146 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * Format a given string.` |
|      - |  406 | ` * The root program.  All variations call this core.` |
|      - |  407 | ` * INPUTS:` |
|      - |  408 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - |  409 | ` *            1. A pointer to the call context.` |
|      - |  410 | ` *            2. A pointer to the list of characters to be output` |
|      - |  411 | ` *               (Note, this list is NOT null terminated.)` |
|      - |  412 | ` *            3. An integer number of characters to be output.` |
|      - |  413 | ` *               (Note: This number might be zero.)` |
|      - |  414 | ` *            4. Upper layer private data.` |
|      - |  415 | ` *   zIn       This is the format string, as in the usual print.` |
|      - |  416 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - |  417 | ` */` |
|   2194 |  418 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - |  419 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - |  420 | `	ph7_context *pCtx,  /* call context */` |
|      - |  421 | `	const char *zIn,    /* Format string */` |
|      - |  422 | `	int nByte,          /* Format string length */` |
|      - |  423 | `	int nArg,           /* Total argument of the given arguments */` |
|      - |  424 | `	ph7_value **apArg,  /* User arguments */` |
|      - |  425 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - |  426 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - |  427 | `	)` |
|      5 |  428 | `{` |
|   2199 |  429 | `	char spaces[] = "                                                  ";` |
|      - |  430 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   2199 |  431 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - |  432 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - |  433 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - |  434 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - |  435 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - |  436 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - |  437 | `	int flag_zeropad;       /* True if the pad character is '0' */` |
|      - |  438 | `	/* php has ONE pad character per specifier, and the LAST pad flag wins:` |
|      - |  439 | `	 * ' ' selects a space (the default), '0' a zero, "'<c>" any byte` |
|      - |  440 | ``	 * (php_formatted_print's single `padding` local). PH7 carried three`` |
|      - |  441 | ``	 * independent flags instead — a `spaces[]` buffer, a `flag_zeropad` and a`` |
|      - |  442 | ``	 * C-style `flag_blanksign` — so "%0 5d" padded with zeros where php pads with`` |
|      - |  443 | `	 * spaces, "%0'x5d" ignored the 'x', and "% d" printed C's space-for-a-positive` |
|      - |  444 | `	 * -sign ("% d" of 5 was " 5" where php answers "5") — php has no such flag. */` |
|      - |  445 | `	char cPad;` |
|      - |  446 | `	ph7_value *pArg;         /* Current processed argument (the scratch COPY below) */` |
|      - |  447 | `	ph7_value *pRawArg;      /* ...and the caller's own value it was copied from */` |
|      - |  448 | ``	int nPos;                /* apArg index a `%N$` selected, or -1 for sequential */`` |
|      - |  449 | `	/* Every conversion below extracts through ph7_value_to_int64 / _to_double /` |
|      - |  450 | `	 * PH7_ValueToStringUV, and all three convert the value they are handed IN` |
|      - |  451 | `	 * PLACE. Handed the caller's own slots that is a write the caller can see, and` |
|      - |  452 | `	 * both ways in were reachable: vsprintf()/vprintf()/vfprintf() pass the` |
|      - |  453 | `	 * ADDRESSES of the $values array's elements (PH7_HashmapValuesToSet), so` |
|      - |  454 | ``	 * `vsprintf("%o %s %b", $a)` retyped $a's elements — int(0), string, int(0) —`` |
|      - |  455 | `	 * where php leaves the array alone; and a POSITIONAL argument reused by a` |
|      - |  456 | `	 * second specifier was read back already converted, so` |
|      - |  457 | ``	 * `sprintf('%1$d\|%1$s', 3.9)` answered "3\|3" for php's "3\|3.9". One scratch`` |
|      - |  458 | `	 * value per call, reloaded per conversion, keeps the formatter read-only.` |
|      - |  459 | `	 * PH7_MemObjLoad points the copy's blob at the source's bytes, so a %s of a` |
|      - |  460 | `	 * long string still costs no copy unless something writes to it. */` |
|      - |  461 | `	ph7_value sScratch;` |
|      - |  462 | `	int bDropDigits;         /* php's explicit-precision-empties-%x/%X/%o/%b rule */` |
|      - |  463 | `	/* A '0'-padded, right-aligned NUMBER puts its sign in front of the padding` |
|      - |  464 | `	 * (php_sprintf_appendstring writes it before the pad run). Holding it here` |
|      - |  465 | `	 * rather than building the zeros into zWorker is what lets the field be wider` |
|      - |  466 | `	 * than the conversion buffer. Zero for every other conversion, including a` |
|      - |  467 | ``	 * `%s` — php passes appendstring `neg = false` there, which is why "%05s" of`` |
|      - |  468 | `	 * "-5" really is "000-5". */` |
|      - |  469 | `	char cLeadSign;` |
|      - |  470 | `	ph7_int64 iVal;` |
|      - |  471 | `	int precision;           /* Precision of the current field */` |
|      - |  472 | ``	/* php only has a precision when a DIGIT follows the '.' (its `expprec`): a bare`` |
|      - |  473 | `` 	 * "%.s" is a precision of zero that nothing consults, so `sprintf("%.s","abc")` `` |
|      - |  474 | ``	 * is "abc" and `sprintf("%.x",42)` is "2a". Reading the dot alone as an`` |
|      - |  475 | `	 * explicit zero truncated both to nothing. */` |
|      - |  476 | `	int bExplicitPrec;` |
|      - |  477 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - |  478 | `	int c,rc,n;` |
|   2199 |  479 | `	sxi32 rcRet = SXRET_OK;   /* Status to hand back through the single exit */` |
|   2199 |  480 | `	ph7_value *pThrowArg = 0; /* First not-stringable %s argument; throws at the end */` |
|      - |  481 | `	int length;              /* Length of the field */` |
|      - |  482 | `	int prefix;` |
|      - |  483 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - |  484 | `	int width;               /* Width of the current field */` |
|      - |  485 | `	int idx;` |
|   2199 |  486 | `	n = (vf == TRUE) ? 0 : 1;` |
|   2199 |  487 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|      - |  488 | `	/* Take the next argument as a scratch COPY: nothing below may write to the` |
|      - |  489 | `	 * caller's value. Answers 0 exactly as the raw form did when the arguments run` |
|      - |  490 | `	 * out (a shortfall is refused by PH7_FormatCheckArgCount before we get here).` |
|      - |  491 | `	 *` |
|      - |  492 | ``	 * A `%N$` specifier reads argument N and leaves the SEQUENTIAL cursor where it`` |
|      - |  493 | ``	 * was — php keeps the two apart (its `currarg` only ever advances for a`` |
|      - |  494 | `	 * specifier that carries no number), and the counting pass above has always` |
|      - |  495 | `	 * modelled it that way. The format loop did not: a positional MOVED the one` |
|      - |  496 | ``	 * cursor, so `sprintf('%1$s\|%s','a','b')` answered "a\|b" for php's "a\|a" and`` |
|      - |  497 | ``	 * `sprintf('%3$s\|%s','a','b','c')` ran off the end of the list it had just`` |
|      - |  498 | `	 * been told was long enough. */` |
|      - |  499 | `#define NEXT_ARG	( pRawArg = (nPos >= 0 ? (nPos < nArg ? apArg[nPos] : 0) \` |
|      - |  500 | `		: (n < nArg ? apArg[n++] : 0)), \` |
|      - |  501 | `	pRawArg ? (PH7_MemObjRelease(&sScratch), \` |
|      - |  502 | `		PH7_MemObjLoad(pRawArg,&sScratch), &sScratch) : 0 )` |
|      - |  503 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - |  504 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - |  505 | `	 * seen here is always valid. */` |
|      - |  506 | `	/* Start the format process */` |
|   4052 |  507 | `	for(;;){` |
|   8109 |  508 | `		zCur = zIn;` |
|  27657 |  509 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  19553 |  510 | `			zIn++;` |
|      5 |  511 | `		}` |
|   8109 |  512 | `		if( zCur < zIn ){` |
|      - |  513 | `			/* Consume chunk verbatim */` |
|   5661 |  514 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   5661 |  515 | `			if( rc != SXRET_OK ){` |
|      - |  516 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 |  517 | `				break;` |
|      - |  518 | `			}` |
|   2828 |  519 | `		}` |
|   8109 |  520 | `		if( zIn >= zEnd ){` |
|      - |  521 | `			/* No more input to process,break immediately */` |
|   2197 |  522 | `			break;` |
|      - |  523 | `		}` |
|      - |  524 | `		/* Find out what flags are present */` |
|   5917 |  525 | `		flag_leftjustify = flag_plussign =` |
|   5912 |  526 | `			flag_alternateform = flag_zeropad = 0;` |
|      - |  527 | `		/* Reset the pad character: a custom pad ('X) from a PREVIOUS specifier must` |
|      - |  528 | `		 * not bleed into this one. php resets it for every specifier. */` |
|   5917 |  529 | `		cPad = ' ';` |
|   5917 |  530 | `		bDropDigits = 0;` |
|   5917 |  531 | `		cLeadSign = 0;` |
|   5917 |  532 | `		nPos = -1;` |
|   5917 |  533 | `		zIn++; /* Jump the precent sign */` |
|   2956 |  534 | `		do{` |
|   8745 |  535 | `			c = zIn[0];` |
|   8745 |  536 | `			switch( c ){` |
|   2461 |  537 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    112 |  538 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|     39 |  539 | `			case ' ':   cPad = ' ';               c = 0;   break;` |
|    180 |  540 | `			case '0':   cPad = '0';               c = 0;   break;` |
|     23 |  541 | `			case '\'':` |
|     47 |  542 | `				zIn++;` |
|     47 |  543 | `				if( zIn < zEnd ){` |
|      - |  544 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     47 |  545 | `					cPad = zIn[0];` |
|     47 |  546 | `					c = 0;` |
|     23 |  547 | `				}` |
|     46 |  548 | `				break;` |
|   5912 |  549 | `			default:                                       break;` |
|      - |  550 | `			}` |
|   8745 |  551 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - |  552 | `		/* Get the field width (saturating — see FormatScanNumber) */` |
|   5917 |  553 | `		width = FormatScanNumber(&zIn,zEnd);` |
|   5917 |  554 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - |  555 | `			/* Position specifer */` |
|     48 |  556 | `			if( width > 0 ){` |
|     48 |  557 | `				nPos = vf ? width - 1 : width;` |
|     23 |  558 | `			}` |
|     48 |  559 | `			zIn++;` |
|     48 |  560 | `			width = 0;` |
|      - |  561 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - |  562 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - |  563 | `			 * not just zero-padding. */` |
|     23 |  564 | `			do{` |
|     50 |  565 | `				c = zIn[0];` |
|     50 |  566 | `				switch( c ){` |
|    ! 0 |  567 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 |  568 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 |  569 | `				case ' ':   cPad = ' ';               c = 0;   break;` |
|    ! 0 |  570 | `				case '0':   cPad = '0';               c = 0;   break;` |
|      1 |  571 | `				case '\'':` |
|      3 |  572 | `					zIn++;` |
|      3 |  573 | `					if( zIn < zEnd ){` |
|      3 |  574 | `						cPad = zIn[0];` |
|      3 |  575 | `						c = 0;` |
|      1 |  576 | `					}` |
|      2 |  577 | `					break;` |
|     46 |  578 | `				default:                                       break;` |
|      - |  579 | `				}` |
|     50 |  580 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     48 |  581 | `			width = FormatScanNumber(&zIn,zEnd);` |
|     23 |  582 | `		}` |
|      - |  583 | `		/* No clamp on the WIDTH: it used to be cut to the conversion buffer` |
|      - |  584 | `		 * (PH7_FMT_BUFSIZ-10 = 1014 bytes) because the zero padding was built` |
|      - |  585 | ``		 * INSIDE that buffer, so `sprintf("%%2000d",5)` answered 1014 characters and`` |
|      - |  586 | ``		 * `%%1100s` silently dropped 86 — a fixed-width record coming out short. The`` |
|      - |  587 | `		 * padding is emitted by the output block below, which chunks it and has no` |
|      - |  588 | `		 * such bound; the two zero-pad-into-zWorker sites are what needed the` |
|      - |  589 | `		 * limit, and both are gone (see cLeadSign). */` |
|      - |  590 | `		/* Get the precision */` |
|   5917 |  591 | `		precision = -1;` |
|   5917 |  592 | `		bExplicitPrec = 0;` |
|   5917 |  593 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    146 |  594 | `			zIn++;` |
|    146 |  595 | `			bExplicitPrec = ( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' );` |
|    146 |  596 | `			precision = FormatScanNumber(&zIn,zEnd);` |
|     72 |  597 | `		}` |
|      - |  598 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - |  599 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - |  600 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   5917 |  601 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 |  602 | `			zIn++;` |
|      4 |  603 | `		}` |
|   5917 |  604 | `		if( zIn >= zEnd ){` |
|      - |  605 | `			/* No more input */` |
|    ! 0 |  606 | `			break;` |
|      - |  607 | `		}` |
|      - |  608 | `		/* Fetch the info entry for the field */` |
|   5917 |  609 | `		pInfo = 0;` |
|   5917 |  610 | `		xtype = PH7_FMT_ERROR;` |
|   5917 |  611 | `		c = zIn[0];` |
|   5917 |  612 | `		zIn++; /* Jump the format specifer */` |
|  13335 |  613 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|  13335 |  614 | `			if( c==aFmt[idx].fmttype ){` |
|   5917 |  615 | `				pInfo = &aFmt[idx];` |
|   5917 |  616 | `				xtype = pInfo->type;` |
|   5917 |  617 | `				break;` |
|      - |  618 | `			}` |
|   3714 |  619 | `		}` |
|   5917 |  620 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   5917 |  621 | `		length = 0;` |
|      - |  622 | `		/* A '0' pad — however it was spelled, "%05d" or the custom "%'05d" — is` |
|      - |  623 | `		 * also php's "put the sign in front of the padding" rule` |
|      - |  624 | ``		 * (php_sprintf_appendstring's `(neg \|\| always_sign) && padding == '0'`),`` |
|      - |  625 | `		 * which is what the two zero-pad blocks below implement. */` |
|   5917 |  626 | `		flag_zeropad = (cPad == '0');` |
|      - |  627 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - |  628 | `		 /*` |
|      - |  629 | `		  ** At this point, variables are initialized as follows:` |
|      - |  630 | `		  **` |
|      - |  631 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - |  632 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - |  633 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - |  634 | `		  **                               field width was negative.` |
|      - |  635 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - |  636 | `		  **                               the conversion character.` |
|      - |  637 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - |  638 | `		  **   width                       The specified field width.  This is` |
|      - |  639 | `		  **                               always non-negative.  Zero is the default.` |
|      - |  640 | `		  **   precision                   The specified precision.  The default` |
|      - |  641 | `		  **                               is -1.` |
|      - |  642 | `		  */` |
|   5917 |  643 | `		switch(xtype){` |
|      5 |  644 | `		case PH7_FMT_PERCENT:` |
|      - |  645 | `			/* A literal percent character */` |
|     11 |  646 | `			zWorker[0] = '%';` |
|     11 |  647 | `			length = (int)sizeof(char);` |
|     11 |  648 | `			break;` |
|      4 |  649 | `		case PH7_FMT_CHARX:` |
|      - |  650 | `			/* The argument is treated as an integer, and presented as the character` |
|      - |  651 | `			 * with that ASCII value` |
|      - |  652 | `			 */` |
|      9 |  653 | `			pArg = NEXT_ARG;` |
|      9 |  654 | `			if( pArg == 0 ){` |
|    ! 0 |  655 | `				c = 0;` |
|    ! 0 |  656 | `			}else{` |
|      9 |  657 | `				c = ph7_value_to_int(pArg);` |
|      - |  658 | `			}` |
|      - |  659 | `			/* NUL byte is an acceptable value */` |
|      9 |  660 | `			zWorker[0] = (char)c;` |
|      9 |  661 | `			length = (int)sizeof(char);` |
|      - |  662 | `			/* php's 'c' is the one conversion with no field: it appends the byte` |
|      - |  663 | `			 * through php_sprintf_appendchar, which takes neither a width nor an` |
|      - |  664 | `			 * alignment, so "%5c" and "%-5c" are both a bare one-byte string. */` |
|      9 |  665 | `			width = 0;` |
|      9 |  666 | `			break;` |
|   2244 |  667 | `		case PH7_FMT_STRING:` |
|      - |  668 | `			/* the argument is treated as and presented as a string */` |
|   4493 |  669 | `			pArg = NEXT_ARG;` |
|   4493 |  670 | `			if( pArg == 0 ){` |
|    ! 0 |  671 | `				length = 0;` |
|   4493 |  672 | `			}else if( PH7_MemObjIsNotStringable(pArg) ){` |
|      - |  673 | `				/* php's user-visible coercion for %s (§2), object half: a class with` |
|      - |  674 | `				 * no __toString() is the catchable "could not be converted to` |
|      - |  675 | `				 * string" Error — but php does NOT let it interrupt the format. The` |
|      - |  676 | `				 * conversion substitutes NOTHING, the format runs to the end, the` |
|      - |  677 | `				 * output is written, and only then does the Error surface. So the` |
|      - |  678 | `				 * throw cannot be RAISED here: PHL's VmThrowException runs an` |
|      - |  679 | `				 * in-place catch immediately, which would print the format's tail` |
|      - |  680 | `				 * after the catch body. Remember the value and throw once the` |
|      - |  681 | `				 * output is out (see the tail of this function). */` |
|     21 |  682 | `				zBuf = "";` |
|     21 |  683 | `				length = 0;` |
|     21 |  684 | `				if( pThrowArg == 0 ){` |
|      - |  685 | `					/* The CALLER's value, not the scratch copy: this one is used after` |
|      - |  686 | `					 * the loop, once the scratch has been reloaded (and released). */` |
|     19 |  687 | `					pThrowArg = pRawArg;` |
|      9 |  688 | `				}` |
|     11 |  689 | `			}else{` |
|      - |  690 | `				/* An ARRAY warns and renders as "Array"; a Stringable renders. */` |
|      - |  691 | `				const char *zSv;` |
|   4473 |  692 | `				sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zSv,&length);` |
|   4473 |  693 | `				zBuf = (char *)zSv;` |
|   4473 |  694 | `				if( rcSv != SXRET_OK ){` |
|      - |  695 | `					/* A __toString() that THREW: unlike the case above this one` |
|      - |  696 | `					 * cannot be predicted, and the throw has already run any` |
|      - |  697 | `					 * in-place catch. Stop formatting rather than emitting the` |
|      - |  698 | `					 * format's tail after the catch body — every other builtin that` |
|      - |  699 | `					 * calls user code (array_map, usort) stops the same way. php` |
|      - |  700 | `					 * keeps going and prints the tail; recorded divergence, and` |
|      - |  701 | `					 * both engines raise the same exception. */` |
|      3 |  702 | `					rcRet = rcSv;` |
|      3 |  703 | `					goto Done;` |
|      - |  704 | `				}` |
|      - |  705 | `			}` |
|   4491 |  706 | `			if( length < 1 ){` |
|      - |  707 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - |  708 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - |  709 | `				 * absent optional part gained a stray space. */` |
|     38 |  710 | `				zBuf = "";` |
|     38 |  711 | `				length = 0;` |
|     18 |  712 | `			}` |
|   4491 |  713 | `			if( bExplicitPrec && precision<length ){` |
|      7 |  714 | `				length = precision;` |
|      3 |  715 | `			}` |
|   4491 |  716 | `			break;` |
|    598 |  717 | `		case PH7_FMT_RADIX: {` |
|      - |  718 | `			/* The digits are produced from an UNSIGNED accumulator. Two php rules` |
|      - |  719 | `			 * ride on that, and the inherited signed one got both wrong:` |
|      - |  720 | `			 *` |
|      - |  721 | `			 *  - only %d is SIGNED. %u/%x/%X/%o/%b REINTERPRET the same 64 bits as` |
|      - |  722 | `			 *    unsigned (php_sprintf_appenduint / php_sprintf_append2n cast to` |
|      - |  723 | `			 *    zend_ulong), so sprintf("%x",-1) is "ffffffffffffffff", not the` |
|      - |  724 | `			 *    magnitude "1" this used to print for every negative value;` |
|      - |  725 | `			 *  - the magnitude of PHP_INT_MIN has no signed representation, so` |
|      - |  726 | ``			 *    `iVal = -iVal` was signed overflow — undefined, and the guard`` |
|      - |  727 | ``			 *    testing for it afterwards (`if( iVal < 0 )`) is exactly what a`` |
|      - |  728 | `			 *    compiler may assume cannot happen. It did: the negative` |
|      - |  729 | ``			 *    accumulator reached `cset[iVal%base]`, indexing BEFORE the digit`` |
|      - |  730 | `			 *    table, so sprintf("%d",PHP_INT_MIN) printed whatever bytes sat` |
|      - |  731 | `			 *    there. Unsigned negation is well-defined for every input.` |
|      - |  732 | `			 */` |
|      - |  733 | `			sxu64 uVal;` |
|   1201 |  734 | `			pArg = NEXT_ARG;` |
|   1201 |  735 | `			if( pArg == 0 ){` |
|    ! 0 |  736 | `				iVal = 0;` |
|    ! 0 |  737 | `			}else{` |
|   1201 |  738 | `				iVal = ph7_value_to_int64(pArg);` |
|      - |  739 | `			}` |
|      - |  740 | `			/* An integer conversion has no PRECISION in php: the '.' part of the` |
|      - |  741 | ``			 * specifier never reaches the digits. `%.5d` of 42 is "42", not the`` |
|      - |  742 | `			 * "00042" C would print — php's php_sprintf_appendint simply is not` |
|      - |  743 | `			 * handed one. For the other radices the same absence is louder:` |
|      - |  744 | `			 * php_sprintf_append2n forwards a max_width of 0 with the` |
|      - |  745 | `			 * "precision was given" flag set, so ANY explicit precision truncates` |
|      - |  746 | ``			 * the digits to nothing and `%.1x` of 42 is the EMPTY string (padded`` |
|      - |  747 | `			 * to $width, which is why "%5.1x" is five spaces). Reproduced rather` |
|      - |  748 | `			 * than smoothed over — parity is binding (§10). */` |
|   1201 |  749 | `			bDropDigits = (bExplicitPrec && pInfo->base != 10);` |
|   1201 |  750 | `			if( precision >= 0 ){` |
|     19 |  751 | `				precision = -1;` |
|      9 |  752 | `			}` |
|      - |  753 | `			/* php's "Can't right-pad 0's on integers" (php_sprintf_appendint, which` |
|      - |  754 | `			 * %u shares) — and only there: %x/%X/%o/%b go through append2n and %e/%f` |
|      - |  755 | `			 * through appenddouble, which both DO right-pad with zeros, so` |
|      - |  756 | `			 * "%-08x" of 5 really is "50000000" while "%-08d" is "5       ".` |
|      - |  757 | `			 * base 10 is exactly the 'd'/'u' pair of the table above. */` |
|   1201 |  758 | `			if( flag_leftjustify && flag_zeropad && pInfo->base == 10 ){` |
|     19 |  759 | `				flag_zeropad = 0;` |
|     19 |  760 | `				cPad = ' ';` |
|      9 |  761 | `			}` |
|      - |  762 | `        /* For the format %#x, the value zero is printed "0" not "0x0". */` |
|   1201 |  763 | `        if( iVal==0 ) flag_alternateform = 0;` |
|   1201 |  764 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|   1039 |  765 | `          if( iVal<0 ){` |
|    116 |  766 | `            uVal = (sxu64)0 - (sxu64)iVal;` |
|    116 |  767 | `            prefix = '-';` |
|     59 |  768 | `          }else{` |
|    925 |  769 | `            uVal = (sxu64)iVal;` |
|      - |  770 | `            /* php's ' ' is a PAD selector, not C's space-for-a-positive-sign, so` |
|      - |  771 | `             * '+' is the only flag that prefixes a non-negative value. */` |
|    925 |  772 | `            prefix = flag_plussign ? '+' : 0;` |
|      - |  773 | `          }` |
|    522 |  774 | `        }else{` |
|    163 |  775 | `			uVal = (sxu64)iVal;` |
|    163 |  776 | `			prefix = 0;` |
|      - |  777 | `		}` |
|      - |  778 | `        /* Zero padding is a RIGHT-aligned idea: it fills between the sign and the` |
|      - |  779 | `         * first digit. Left-aligned, php pads on the far side like any other pad` |
|      - |  780 | `         * character (append2n hands the '0' straight to appendstring's ALIGN_LEFT` |
|      - |  781 | `         * arm), so "%-08x" of 5 is "50000000". */` |
|   1201 |  782 | `        if( flag_zeropad && !flag_leftjustify ){` |
|    112 |  783 | `          cLeadSign = (char)prefix;` |
|    112 |  784 | `          prefix = 0;` |
|     55 |  785 | `        }` |
|   1201 |  786 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - |  787 | `        {` |
|      - |  788 | `          const char *cset;` |
|      - |  789 | `          sxu64 base;` |
|   1201 |  790 | `          cset = pInfo->charset;` |
|   1201 |  791 | `          base = (sxu64)pInfo->base;` |
|    598 |  792 | `          do{                                           /* Convert to ascii */` |
|   4009 |  793 | `            *(--zBuf) = cset[uVal%base];` |
|   4009 |  794 | `            uVal = uVal/base;` |
|   4009 |  795 | `          }while( uVal>0 );` |
|      - |  796 | `        }` |
|   1201 |  797 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|      - |  798 | `        /* No zero fill here: a radix conversion has no precision to fill to, and` |
|      - |  799 | `         * the '0' pad is the output block's job now (see cLeadSign). */` |
|   1201 |  800 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|   1201 |  801 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - |  802 | `          char *pre, x;` |
|    ! 0 |  803 | `          pre = pInfo->prefix;` |
|    ! 0 |  804 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 |  805 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 |  806 | `          }` |
|    ! 0 |  807 | `        }` |
|   1201 |  808 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   1201 |  809 | `		if( bDropDigits ){` |
|      9 |  810 | `			length = 0;` |
|      4 |  811 | `		}` |
|   1201 |  812 | `		break;` |
|      - |  813 | `		}` |
|    105 |  814 | `		case PH7_FMT_FLOAT:` |
|      - |  815 | `		case PH7_FMT_EXP:` |
|      - |  816 | `		case PH7_FMT_GENERIC:{` |
|      - |  817 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  818 | `		double realvalue;` |
|      - |  819 | `		char zFmt[8];` |
|      - |  820 | `		int nOut, nFmt;` |
|    212 |  821 | `		pArg = NEXT_ARG;` |
|    212 |  822 | `		if( pArg == 0 ){` |
|    ! 0 |  823 | `			realvalue = 0;` |
|    ! 0 |  824 | `		}else{` |
|    212 |  825 | `			realvalue = ph7_value_to_double(pArg);` |
|      - |  826 | `		}` |
|      - |  827 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - |  828 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    212 |  829 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 |  830 | `			zBuf = "NaN";` |
|     21 |  831 | `			length = 3;` |
|     21 |  832 | `			width = 0;` |
|     21 |  833 | `			break;` |
|      - |  834 | `		}` |
|    192 |  835 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 |  836 | `			if( realvalue < 0.0 ){` |
|     15 |  837 | `				zBuf = "-INF";` |
|     15 |  838 | `				length = 4;` |
|      8 |  839 | `			}else{` |
|     23 |  840 | `				zBuf = "INF";` |
|     23 |  841 | `				length = 3;` |
|      - |  842 | `			}` |
|     37 |  843 | `			width = 0;` |
|     37 |  844 | `			break;` |
|      - |  845 | `		}` |
|    156 |  846 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    156 |  847 | `		if( precision > 53 ){` |
|      - |  848 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - |  849 | `			 * (message prefixed with the active function's name, like` |
|      - |  850 | `			 * php_error_docref). */` |
|      - |  851 | `			char zMsg[160];` |
|      4 |  852 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  853 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 |  854 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 |  855 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 |  856 | `			precision = 53;` |
|      1 |  857 | `		}` |
|      - |  858 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - |  859 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    156 |  860 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 |  861 | `			realvalue = 0.0;` |
|      4 |  862 | `		}` |
|      - |  863 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - |  864 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - |  865 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - |  866 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - |  867 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    156 |  868 | `		nFmt = 0;` |
|    156 |  869 | `		zFmt[nFmt++] = '%';` |
|    156 |  870 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - |  871 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - |  872 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    156 |  873 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    156 |  874 | `		zFmt[nFmt++] = '.';` |
|    156 |  875 | `		zFmt[nFmt++] = '*';` |
|    204 |  876 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 |  877 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 |  878 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    156 |  879 | `		zFmt[nFmt] = 0;` |
|    156 |  880 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    156 |  881 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - |  882 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - |  883 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 |  884 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 |  885 | `		}` |
|    156 |  886 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    156 |  887 | `		zBuf = zWorker;` |
|    156 |  888 | `		length = nOut;` |
|      - |  889 | `		/* The zero padding goes between the sign snprintf wrote and the first` |
|      - |  890 | `		 * digit, so hand the sign to the output block and leave the rest here. */` |
|    154 |  891 | `		if( flag_zeropad && !flag_leftjustify` |
|     22 |  892 | `		 && (zWorker[0]=='-' \|\| zWorker[0]=='+') ){` |
|     11 |  893 | `			cLeadSign = zWorker[0];` |
|     11 |  894 | `			zBuf++;` |
|     11 |  895 | `			length--;` |
|      5 |  896 | `		}` |
|      - |  897 | `#else` |
|      - |  898 | `         zBuf = " ";` |
|      - |  899 | `		 length = (int)sizeof(char);` |
|      - |  900 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    156 |  901 | `		 break;` |
|      - |  902 | `							 }` |
|    ! 0 |  903 | `		default:` |
|      - |  904 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - |  905 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - |  906 | `			 * no-op that emits nothing. */` |
|    ! 0 |  907 | `			length = 0;` |
|    ! 0 |  908 | `			break;` |
|      - |  909 | `		}` |
|      - |  910 | `		 /*` |
|      - |  911 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - |  912 | `		 ** "length" characters long.The field width is "width".Do` |
|      - |  913 | `		 ** the output.` |
|      - |  914 | `		 */` |
|   5915 |  915 | `    if( cLeadSign ){` |
|      - |  916 | `      /* php writes the sign ahead of a '0' pad run; it fills one byte of the` |
|      - |  917 | `       * field, so the padding below has that much less to do. */` |
|     37 |  918 | `      rc = xConsumer(pCtx,&cLeadSign,1,pUserData);` |
|     37 |  919 | `      if( rc != SXRET_OK ){` |
|    ! 0 |  920 | `        rcRet = SXERR_ABORT;` |
|    ! 0 |  921 | `        goto Done;` |
|      - |  922 | `      }` |
|     37 |  923 | `      width--;` |
|     18 |  924 | `    }` |
|   5915 |  925 | `    if( width > length ){` |
|      - |  926 | `      /* Fill the pad buffer with THIS specifier's pad character. */` |
| 124751 |  927 | `      for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = cPad; }` |
|   1223 |  928 | `    }` |
|   5915 |  929 | `    if( !flag_leftjustify ){` |
|      - |  930 | `      register int nspace;` |
|   3459 |  931 | `      nspace = width-length;` |
|   3459 |  932 | `      if( nspace>0 ){` |
|   2335 |  933 | `        while( nspace>=etSPACESIZE ){` |
|   2103 |  934 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   2103 |  935 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  936 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|    ! 0 |  937 | `				goto Done;` |
|      - |  938 | `			}` |
|   2103 |  939 | `			nspace -= etSPACESIZE;` |
|      1 |  940 | `        }` |
|    233 |  941 | `        if( nspace>0 ){` |
|    233 |  942 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    233 |  943 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  944 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|    ! 0 |  945 | `				goto Done;` |
|      - |  946 | `			}` |
|    115 |  947 | `		}` |
|    115 |  948 | `      }` |
|   1727 |  949 | `    }` |
|   5915 |  950 | `    if( length>0 ){` |
|   5869 |  951 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   5869 |  952 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  953 | `		  rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|    ! 0 |  954 | `		  goto Done;` |
|      - |  955 | `		}` |
|   2932 |  956 | `    }` |
|   5915 |  957 | `    if( flag_leftjustify ){` |
|      - |  958 | `      register int nspace;` |
|   2461 |  959 | `      nspace = width-length;` |
|   2461 |  960 | `      if( nspace>0 ){` |
|   2689 |  961 | `        while( nspace>=etSPACESIZE ){` |
|    469 |  962 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    469 |  963 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  964 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|    ! 0 |  965 | `				goto Done;` |
|      - |  966 | `			}` |
|    469 |  967 | `			nspace -= etSPACESIZE;` |
|      1 |  968 | `        }` |
|   2221 |  969 | `        if( nspace>0 ){` |
|   2221 |  970 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|   2221 |  971 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  972 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|    ! 0 |  973 | `				goto Done;` |
|      - |  974 | `			}` |
|   1108 |  975 | `		}` |
|   1108 |  976 | `      }` |
|   1228 |  977 | `    }` |
|      5 |  978 | ` }/* for(;;) */` |
|   2197 |  979 | `	if( pThrowArg ){` |
|      - |  980 | `		/* The format ran to completion and its output is out; raise php's Error` |
|      - |  981 | ``		 * now. `printf("A[%s]B", new P())` prints "A[]B" and THEN throws, while`` |
|      - |  982 | `		 * sprintf()'s finished result is simply discarded by the unwind. */` |
|     19 |  983 | `		PH7_MemObjRelease(&sScratch);` |
|     19 |  984 | `		return PH7_MemObjToStringUV(pThrowArg);` |
|      - |  985 | `	}` |
|   1087 |  986 | `Done:` |
|      - |  987 | `	/* Single exit: the scratch copy holds a reference on an array/instance` |
|      - |  988 | `	 * argument it was loaded from, so every way out releases it. */` |
|   2181 |  989 | `	PH7_MemObjRelease(&sScratch);` |
|   2181 |  990 | `	return rcRet;` |
|   1102 |  991 | `}` |
|      - |  992 | `/*` |
|      - |  993 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - |  994 | ` */` |
|   4680 |  995 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      4 |  996 | `{` |
|      - |  997 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - |  998 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - |  999 | `	 * non-OK rc also stops the format loop. */` |
|   4684 | 1000 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|   4684 | 1001 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|   4684 | 1002 | `	return *pRc;` |
|      4 | 1003 | `}` |
|      - | 1004 | `/*` |
|      - | 1005 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 1006 | ` *  Return a formatted string.` |
|      - | 1007 | ` * Parameters` |
|      - | 1008 | ` *  $format` |
|      - | 1009 | ` *    The format string (see block comment above)` |
|      - | 1010 | ` * Return` |
|      - | 1011 | ` *  A string produced according to the formatting string format.` |
|      - | 1012 | ` */` |
|    668 | 1013 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1014 | `{` |
|      - | 1015 | `	sxi32 rcFmt;` |
|      - | 1016 | `	const char *zFormat;` |
|    672 | 1017 | `	sxi32 rc = SXRET_OK;` |
|      - | 1018 | `	int nLen;` |
|    672 | 1019 | `	if( nArg < 1 ){` |
|      - | 1020 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 1021 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1022 | `		return PH7_OK;` |
|      - | 1023 | `	}` |
|      - | 1024 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    672 | 1025 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    672 | 1026 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1027 | `		return rc;` |
|      - | 1028 | `	}` |
|      - | 1029 | `	/* Extract the string format (scalars/null coerce). */` |
|    672 | 1030 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    672 | 1031 | `	if( nLen < 1 ){` |
|      - | 1032 | `		/* Empty string */` |
|    ! 0 | 1033 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1034 | `		return PH7_OK;` |
|      - | 1035 | `	}` |
|      - | 1036 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 1037 | `	 * output; propagate the throw status verbatim. */` |
|    672 | 1038 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    672 | 1039 | `	if( rc != PH7_OK ){` |
|     41 | 1040 | `		return rc;` |
|      - | 1041 | `	}` |
|      - | 1042 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    632 | 1043 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    632 | 1044 | `	if( rc != PH7_OK ){` |
|     31 | 1045 | `		return rc;` |
|      - | 1046 | `	}` |
|      - | 1047 | `	/* Seed the result with the empty string: a format whose every conversion` |
|      - | 1048 | `	 * substitutes NOTHING ("%s" of "", false or null) never calls the consumer at` |
|      - | 1049 | `	 * all, and an untouched return value is NULL — so sprintf("%s","") answered` |
|      - | 1050 | `	 * NULL where php answers "". */` |
|    602 | 1051 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1052 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    602 | 1053 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    602 | 1054 | `	if( rc != SXRET_OK ){` |
|      - | 1055 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 1056 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 1057 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1058 | `	}` |
|      - | 1059 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1060 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1061 | `	 * that), so report the throw last. */` |
|    602 | 1062 | `	if( rcFmt != SXRET_OK ){` |
|     12 | 1063 | `		pCtx->nThrowRc = rcFmt;` |
|     12 | 1064 | `		return rcFmt;` |
|      - | 1065 | `	}` |
|    592 | 1066 | `	return PH7_OK;` |
|    338 | 1067 | `}` |
|      - | 1068 | `/*` |
|      - | 1069 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 1070 | ` */` |
|  11854 | 1071 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      5 | 1072 | `{` |
|  11859 | 1073 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 1074 | `	/* Call the VM output consumer directly */` |
|  11859 | 1075 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 1076 | `	/* Increment counter */` |
|  11859 | 1077 | `	*pCounter += nLen;` |
|  11859 | 1078 | `	return PH7_OK;` |
|      5 | 1079 | `}` |
|      - | 1080 | `/*` |
|      - | 1081 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 1082 | ` *  Output a formatted string.` |
|      - | 1083 | ` * Parameters` |
|      - | 1084 | ` *  $format` |
|      - | 1085 | ` *   See sprintf() for a description of format.` |
|      - | 1086 | ` * Return` |
|      - | 1087 | ` *  The length of the outputted string.` |
|      - | 1088 | ` */` |
|   1558 | 1089 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1090 | `{` |
|      - | 1091 | `	sxi32 rcFmt;` |
|   1563 | 1092 | `	ph7_int64 nCounter = 0;` |
|      - | 1093 | `	const char *zFormat;` |
|      - | 1094 | `	int nLen;` |
|   1563 | 1095 | `	if( nArg < 1 ){` |
|      - | 1096 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 1097 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1098 | `		return PH7_OK;` |
|      - | 1099 | `	}` |
|      - | 1100 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 1101 | `	{` |
|   1563 | 1102 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   1563 | 1103 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 1104 | `			return rcf;` |
|      - | 1105 | `		}` |
|      - | 1106 | `	}` |
|      - | 1107 | `	/* Extract the string format (scalars/null coerce). */` |
|   1563 | 1108 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   1563 | 1109 | `	if( nLen < 1 ){` |
|      - | 1110 | `		/* Empty string */` |
|    ! 0 | 1111 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1112 | `		return PH7_OK;` |
|      - | 1113 | `	}` |
|      - | 1114 | `	{` |
|      - | 1115 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|      - | 1116 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|   1563 | 1117 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   1563 | 1118 | `		if( rcv != PH7_OK ){` |
|      3 | 1119 | `			return rcv;` |
|      - | 1120 | `		}` |
|      - | 1121 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 1122 | `		 * before any output; propagate the throw status verbatim. */` |
|   1561 | 1123 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   1561 | 1124 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 1125 | `			return rcv;` |
|      - | 1126 | `		}` |
|      - | 1127 | `	}` |
|      - | 1128 | `	/* Format the string */` |
|   1561 | 1129 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 1130 | `	/* Return the length of the outputted string */` |
|   1561 | 1131 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 1132 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1133 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1134 | `	 * that), so report the throw last. */` |
|   1561 | 1135 | `	if( rcFmt != SXRET_OK ){` |
|      3 | 1136 | `		pCtx->nThrowRc = rcFmt;` |
|      3 | 1137 | `		return rcFmt;` |
|      - | 1138 | `	}` |
|   1559 | 1139 | `	return PH7_OK;` |
|    784 | 1140 | `}` |
|      - | 1141 | `/*` |
|      - | 1142 | ` * int vprintf(string $format,array $args)` |
|      - | 1143 | ` *  Output a formatted string.` |
|      - | 1144 | ` * Parameters` |
|      - | 1145 | ` *  $format` |
|      - | 1146 | ` *   See sprintf() for a description of format.` |
|      - | 1147 | ` * Return` |
|      - | 1148 | ` *  The length of the outputted string.` |
|      - | 1149 | ` */` |
|      6 | 1150 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1151 | `{` |
|      - | 1152 | `	sxi32 rcFmt;` |
|      8 | 1153 | `	ph7_int64 nCounter = 0;` |
|      - | 1154 | `	const char *zFormat;` |
|      - | 1155 | `	ph7_hashmap *pMap;` |
|      - | 1156 | `	SySet sArg;` |
|      - | 1157 | `	int nLen,n;` |
|      8 | 1158 | `	if( nArg < 2 ){` |
|      - | 1159 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 1160 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1161 | `		return PH7_OK;` |
|      - | 1162 | `	}` |
|      - | 1163 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      8 | 1164 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      8 | 1165 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 1166 | `		return rcFmt;` |
|      - | 1167 | `	}` |
|      8 | 1168 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 1169 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 1170 | `		char zBuf[64];` |
|    ! 0 | 1171 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1172 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|    ! 0 | 1173 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1174 | `	}` |
|      - | 1175 | `	/* Extract the string format (scalars/null coerce). */` |
|      8 | 1176 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      8 | 1177 | `	if( nLen < 1 ){` |
|      - | 1178 | `		/* Empty string */` |
|    ! 0 | 1179 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1180 | `		return PH7_OK;` |
|      - | 1181 | `	}` |
|      - | 1182 | `	/* Point to the hashmap */` |
|      8 | 1183 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 1184 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 1185 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|      - | 1186 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|      - | 1187 | `	 * rather than the missing specifier. */` |
|      8 | 1188 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      8 | 1189 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 1190 | `		return rcFmt;` |
|      - | 1191 | `	}` |
|      - | 1192 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 1193 | `	 * any output; propagate the throw status verbatim. */` |
|      8 | 1194 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      8 | 1195 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 1196 | `		return rcFmt;` |
|      - | 1197 | `	}` |
|      - | 1198 | `	/* Extract arguments from the hashmap */` |
|      8 | 1199 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 1200 | `	/* Format the string */` |
|      8 | 1201 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 1202 | `	/* Release the container */` |
|      8 | 1203 | `	SySetRelease(&sArg);` |
|      - | 1204 | `	/* Return the length of the outputted string */` |
|      8 | 1205 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 1206 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1207 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1208 | `	 * that), so report the throw last. */` |
|      8 | 1209 | `	if( rcFmt != SXRET_OK ){` |
|      3 | 1210 | `		pCtx->nThrowRc = rcFmt;` |
|      3 | 1211 | `		return rcFmt;` |
|      - | 1212 | `	}` |
|      5 | 1213 | `	return PH7_OK;` |
|      5 | 1214 | `}` |
|      - | 1215 | `/*` |
|      - | 1216 | ` * int vsprintf(string $format,array $args)` |
|      - | 1217 | ` *  Output a formatted string.` |
|      - | 1218 | ` * Parameters` |
|      - | 1219 | ` *  $format` |
|      - | 1220 | ` *   See sprintf() for a description of format.` |
|      - | 1221 | ` * Return` |
|      - | 1222 | ` *  A string produced according to the formatting string format.` |
|      - | 1223 | ` */` |
|     24 | 1224 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1225 | `{` |
|      - | 1226 | `	sxi32 rcFmt;` |
|      - | 1227 | `	const char *zFormat;` |
|      - | 1228 | `	ph7_hashmap *pMap;` |
|      - | 1229 | `	SySet sArg;` |
|     26 | 1230 | `	sxi32 rc = SXRET_OK;` |
|      - | 1231 | `	int nLen,n;` |
|     26 | 1232 | `	if( nArg < 2 ){` |
|      - | 1233 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 1234 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1235 | `		return PH7_OK;` |
|      - | 1236 | `	}` |
|      - | 1237 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     26 | 1238 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     26 | 1239 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1240 | `		return rc;` |
|      - | 1241 | `	}` |
|     26 | 1242 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 1243 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 1244 | `		char zBuf[64];` |
|    ! 0 | 1245 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1246 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|    ! 0 | 1247 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1248 | `	}` |
|      - | 1249 | `	/* Extract the string format (scalars/null coerce). */` |
|     26 | 1250 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     26 | 1251 | `	if( nLen < 1 ){` |
|      - | 1252 | `		/* Empty string */` |
|    ! 0 | 1253 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1254 | `		return PH7_OK;` |
|      - | 1255 | `	}` |
|      - | 1256 | `	/* Point to hashmap */` |
|     26 | 1257 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 1258 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 1259 | `	 * php runs this BEFORE validating the specifiers. */` |
|     26 | 1260 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     26 | 1261 | `	if( rcFmt != PH7_OK ){` |
|      7 | 1262 | `		return rcFmt;` |
|      - | 1263 | `	}` |
|      - | 1264 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 1265 | `	 * any output; propagate the throw status verbatim. */` |
|     20 | 1266 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     20 | 1267 | `	if( rcFmt != PH7_OK ){` |
|      3 | 1268 | `		return rcFmt;` |
|      - | 1269 | `	}` |
|      - | 1270 | `	/* Extract arguments from the hashmap */` |
|     18 | 1271 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 1272 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      - | 1273 | `	/* Empty-result seed — see PH7_builtin_sprintf. */` |
|     18 | 1274 | `	ph7_result_string(pCtx,"",0);` |
|     18 | 1275 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 1276 | `	/* Release the container */` |
|     18 | 1277 | `	SySetRelease(&sArg);` |
|     18 | 1278 | `	if( rc != SXRET_OK ){` |
|      - | 1279 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 1280 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1281 | `	}` |
|      - | 1282 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1283 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1284 | `	 * that), so report the throw last. */` |
|     18 | 1285 | `	if( rcFmt != SXRET_OK ){` |
|      3 | 1286 | `		pCtx->nThrowRc = rcFmt;` |
|      3 | 1287 | `		return rcFmt;` |
|      - | 1288 | `	}` |
|     15 | 1289 | `	return PH7_OK;` |
|     14 | 1290 | `}` |
|      - | 1291 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1292 |  |
