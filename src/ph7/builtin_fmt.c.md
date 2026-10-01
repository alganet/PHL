# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 545/622 lines (87.62%)

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
|   17110 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|       5 |   95 | `{` |
|   17115 |   96 | `	const char *zEnd = &zIn[nByte];` |
|       - |   97 | `	int c,idx;` |
|  201019 |   98 | `	while( zIn < zEnd ){` |
|  183947 |   99 | `		if( zIn[0] != '%' ){` |
|  135865 |  100 | `			zIn++;` |
|  135865 |  101 | `			continue;` |
|       - |  102 | `		}` |
|   48087 |  103 | `		zIn++; /* jump the percent sign */` |
|       - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|       - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|       - |  106 | `		 * unknown specifier, matching php. */` |
|   69324 |  107 | `		while( zIn < zEnd ){` |
|   69312 |  108 | `			c = zIn[0];` |
|   69312 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   21194 |  110 | `				zIn++;` |
|   21194 |  111 | `				continue;` |
|       - |  112 | `			}` |
|   48123 |  113 | `			if( c=='\'' ){` |
|      49 |  114 | `				zIn++;` |
|      49 |  115 | `				if( zIn < zEnd ){` |
|      49 |  116 | `					zIn++; /* the custom pad character */` |
|      24 |  117 | `				}` |
|      49 |  118 | `				continue;` |
|       - |  119 | `			}` |
|   48075 |  120 | `			break;` |
|     ! 0 |  121 | `		}` |
|       - |  122 | `		/* field width */` |
|   85552 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   37470 |  124 | `			zIn++;` |
|       5 |  125 | `		}` |
|       - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|       - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   48087 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
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
|   48087 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     315 |  151 | `			zIn++;` |
|     803 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     491 |  153 | `				zIn++;` |
|       3 |  154 | `			}` |
|     156 |  155 | `		}` |
|       - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   48087 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|      11 |  158 | `			zIn++;` |
|       5 |  159 | `		}` |
|   48087 |  160 | `		if( zIn >= zEnd ){` |
|       - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|       - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|      17 |  163 | `			*pbDangling = TRUE;` |
|      17 |  164 | `			return FALSE;` |
|       - |  165 | `		}` |
|   48071 |  166 | `		c = zIn[0];` |
|   48071 |  167 | `		zIn++; /* jump the conversion specifier */` |
|   96886 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   96864 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   48049 |  170 | `				break;` |
|       - |  171 | `			}` |
|   24100 |  172 | `		}` |
|   48071 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|      23 |  174 | `			*pBad = c; /* unknown specifier */` |
|      23 |  175 | `			return TRUE;` |
|       - |  176 | `		}` |
|       5 |  177 | `	}` |
|   17077 |  178 | `	return FALSE;` |
|    8390 |  179 | `}` |
|       - |  180 | `/*` |
|       - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|       - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|       - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|       - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|       - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|       - |  186 | ` * Returns PH7_OK when the format is valid.` |
|       - |  187 | ` */` |
|   17110 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|       5 |  189 | `{` |
|   17115 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   17115 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|      34 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      11 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|       - |  194 | `	}` |
|   17093 |  195 | `	if( bDangling ){` |
|      17 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  197 | `			"Missing format specifier at end of string");` |
|       - |  198 | `	}` |
|   17077 |  199 | `	return PH7_OK;` |
|    8390 |  200 | `}` |
|       - |  201 | `/*` |
|       - |  202 | ` * Read a run of decimal digits, saturating at PH7_FMT_NUM_CAP rather than` |
|       - |  203 | `` * wrapping: `%99999999999999999999d` must be REPORTED, and a signed overflow on`` |
|       - |  204 | ` * the way to reporting it is undefined behaviour (it used to make the width come` |
|       - |  205 | ` * out negative, or a positional index come out as an ordinary sequential one).` |
|       - |  206 | ` */` |
|       - |  207 | `#define PH7_FMT_NUM_CAP 2147483647` |
|   96920 |  208 | `static int FormatScanNumber(const char **pzIn,const char *zEnd)` |
|       5 |  209 | `{` |
|   96925 |  210 | `	const char *zIn = *pzIn;` |
|   96925 |  211 | `	int v = 0;` |
|  173013 |  212 | `	while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   76093 |  213 | `		int d = zIn[0]-'0';` |
|       - |  214 | `		/* Tested BEFORE the multiply: a signed overflow is undefined, so a guard` |
|       - |  215 | `		 * that inspects the wrapped result is one an optimiser may delete — and` |
|       - |  216 | ``		 * did, which is how `%2147483648d` slipped past the range check below. */`` |
|   76093 |  217 | `		if( v > (PH7_FMT_NUM_CAP - d)/10 ){` |
|      29 |  218 | `			v = PH7_FMT_NUM_CAP;` |
|      15 |  219 | `		}else{` |
|   76065 |  220 | `			v = v*10 + d;` |
|       - |  221 | `		}` |
|   76093 |  222 | `		zIn++;` |
|       5 |  223 | `	}` |
|   96925 |  224 | `	*pzIn = zIn;` |
|   96925 |  225 | `	return v;` |
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
|   17162 |  247 | `static int FormatRequiredArgs(const char *zIn,int nByte,const char **pzBad)` |
|       5 |  248 | `{` |
|   17167 |  249 | `	const char *zEnd = &zIn[nByte];` |
|   17167 |  250 | `	int c,seq = 0,maxpos = 0;` |
|  201173 |  251 | `	while( zIn < zEnd ){` |
|  184057 |  252 | `		int numVal = 0,pos = 0;` |
|  184057 |  253 | `		if( zIn[0] != '%' ){` |
|  135905 |  254 | `			zIn++;` |
|  135905 |  255 | `			continue;` |
|       - |  256 | `		}` |
|   48157 |  257 | `		zIn++; /* jump the percent sign */` |
|       - |  258 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   69398 |  259 | `		while( zIn < zEnd ){` |
|   69380 |  260 | `			c = zIn[0];` |
|   69380 |  261 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|   48187 |  262 | `			if( c=='\'' ){` |
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
|   48135 |  273 | `			break;` |
|     ! 0 |  274 | `		}` |
|       - |  275 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   48153 |  276 | `		numVal = FormatScanNumber(&zIn,zEnd);` |
|   48153 |  277 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
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
|   48143 |  304 | `		if( numVal >= PH7_FMT_NUM_CAP ){` |
|       7 |  305 | `			*pzBad = PH7_FMT_BAD_WIDTH;` |
|       7 |  306 | `			return seq;` |
|       - |  307 | `		}` |
|       - |  308 | `		/* precision */` |
|   48137 |  309 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     319 |  310 | `			zIn++;` |
|     319 |  311 | `			if( FormatScanNumber(&zIn,zEnd) >= PH7_FMT_NUM_CAP ){` |
|       5 |  312 | `				*pzBad = PH7_FMT_BAD_PRECISION;` |
|       5 |  313 | `				return seq;` |
|       - |  314 | `			}` |
|     156 |  315 | `		}` |
|       - |  316 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   48133 |  317 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|   48133 |  318 | `		if( zIn >= zEnd ){` |
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
|   48111 |  329 | `		c = zIn[0];` |
|   48111 |  330 | `		zIn++; /* jump the conversion specifier */` |
|   48111 |  331 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   47907 |  332 | `		if( pos > 0 ){` |
|      56 |  333 | `			if( pos > maxpos ){ maxpos = pos; }` |
|      29 |  334 | `		}else{` |
|   47853 |  335 | `			seq++;` |
|       - |  336 | `		}` |
|       5 |  337 | `	}` |
|   17143 |  338 | `	return seq > maxpos ? seq : maxpos;` |
|    8416 |  339 | `}` |
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
|   17162 |  352 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|       5 |  353 | `{` |
|   17167 |  354 | `	const char *zBad = 0;` |
|   17167 |  355 | `	int required = FormatRequiredArgs(zFormat,nByte,&zBad);` |
|   17167 |  356 | `	if( zBad ){` |
|       - |  357 | `		/* php refuses a specifier's own shape before it counts the values, so` |
|       - |  358 | ``		 * `sprintf("%2$s%0$s","a")` and `sprintf("%'","a")` are the ValueError and`` |
|       - |  359 | `		 * not the (also true) ArgumentCountError. */` |
|      25 |  360 | `		return PH7_VmThrowException(pCtx,"ValueError","%s",zBad);` |
|       - |  361 | `	}` |
|   17143 |  362 | `	if( nValues < required ){` |
|      29 |  363 | `		if( bVararg ){` |
|      13 |  364 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       4 |  365 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|       - |  366 | `		}` |
|      31 |  367 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      10 |  368 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|       - |  369 | `	}` |
|   17115 |  370 | `	return PH7_OK;` |
|    8416 |  371 | `}` |
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
|   17166 |  394 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|       5 |  395 | `{` |
|   17171 |  396 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|       - |  397 | `		char zBuf[64];` |
|     ! 0 |  398 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  399 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|     ! 0 |  400 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|       - |  401 | `	}` |
|   17171 |  402 | `	return PH7_OK;` |
|    8418 |  403 | `}` |
|       - |  404 | `/*` |
|       - |  405 | ` * Format a given string.` |
|       - |  406 | ` * The root program.  All variations call this core.` |
|       - |  407 | ` * INPUTS:` |
|       - |  408 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|       - |  409 | ` *            1. A pointer to the call context.` |
|       - |  410 | ` *            2. A pointer to the list of characters to be output` |
|       - |  411 | ` *               (Note, this list is NOT null terminated.)` |
|       - |  412 | ` *            3. An integer number of characters to be output.` |
|       - |  413 | ` *               (Note: This number might be zero.)` |
|       - |  414 | ` *            4. Upper layer private data.` |
|       - |  415 | ` *   zIn       This is the format string, as in the usual print.` |
|       - |  416 | ` *   apArg     This is a pointer to a list of arguments.` |
|       - |  417 | ` */` |
|   17072 |  418 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|       - |  419 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|       - |  420 | `	ph7_context *pCtx,  /* call context */` |
|       - |  421 | `	const char *zIn,    /* Format string */` |
|       - |  422 | `	int nByte,          /* Format string length */` |
|       - |  423 | `	int nArg,           /* Total argument of the given arguments */` |
|       - |  424 | `	ph7_value **apArg,  /* User arguments */` |
|       - |  425 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|       - |  426 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|       - |  427 | `	)` |
|       5 |  428 | `{` |
|   17077 |  429 | `	char spaces[] = "                                                  ";` |
|       - |  430 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   17077 |  431 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|       - |  432 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|       - |  433 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|       - |  434 | `	int flag_alternateform; /* True if "#" flag is present */` |
|       - |  435 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|       - |  436 | `	int flag_plussign;      /* True if "+" flag is present */` |
|       - |  437 | `	int flag_zeropad;       /* True if the pad character is '0' */` |
|       - |  438 | `	/* php has ONE pad character per specifier, and the LAST pad flag wins:` |
|       - |  439 | `	 * ' ' selects a space (the default), '0' a zero, "'<c>" any byte` |
|       - |  440 | ``	 * (php_formatted_print's single `padding` local). PH7 carried three`` |
|       - |  441 | ``	 * independent flags instead — a `spaces[]` buffer, a `flag_zeropad` and a`` |
|       - |  442 | ``	 * C-style `flag_blanksign` — so "%0 5d" padded with zeros where php pads with`` |
|       - |  443 | `	 * spaces, "%0'x5d" ignored the 'x', and "% d" printed C's space-for-a-positive` |
|       - |  444 | `	 * -sign ("% d" of 5 was " 5" where php answers "5") — php has no such flag. */` |
|       - |  445 | `	char cPad;` |
|       - |  446 | `	ph7_value *pArg;         /* Current processed argument (the scratch COPY below) */` |
|       - |  447 | `	ph7_value *pRawArg;      /* ...and the caller's own value it was copied from */` |
|       - |  448 | ``	int nPos;                /* apArg index a `%N$` selected, or -1 for sequential */`` |
|       - |  449 | `	/* Every conversion below extracts through ph7_value_to_int64 / _to_double /` |
|       - |  450 | `	 * PH7_ValueToStringUV, and all three convert the value they are handed IN` |
|       - |  451 | `	 * PLACE. Handed the caller's own slots that is a write the caller can see, and` |
|       - |  452 | `	 * both ways in were reachable: vsprintf()/vprintf()/vfprintf() pass the` |
|       - |  453 | `	 * ADDRESSES of the $values array's elements (PH7_HashmapValuesToSet), so` |
|       - |  454 | ``	 * `vsprintf("%o %s %b", $a)` retyped $a's elements — int(0), string, int(0) —`` |
|       - |  455 | `	 * where php leaves the array alone; and a POSITIONAL argument reused by a` |
|       - |  456 | `	 * second specifier was read back already converted, so` |
|       - |  457 | ``	 * `sprintf('%1$d\|%1$s', 3.9)` answered "3\|3" for php's "3\|3.9". One scratch`` |
|       - |  458 | `	 * value per call, reloaded per conversion, keeps the formatter read-only.` |
|       - |  459 | `	 * PH7_MemObjLoad points the copy's blob at the source's bytes, so a %s of a` |
|       - |  460 | `	 * long string still costs no copy unless something writes to it. */` |
|       - |  461 | `	ph7_value sScratch;` |
|       - |  462 | `	int bDropDigits;         /* php's explicit-precision-empties-%x/%X/%o/%b rule */` |
|       - |  463 | `	/* A '0'-padded, right-aligned NUMBER puts its sign in front of the padding` |
|       - |  464 | `	 * (php_sprintf_appendstring writes it before the pad run). Holding it here` |
|       - |  465 | `	 * rather than building the zeros into zWorker is what lets the field be wider` |
|       - |  466 | `	 * than the conversion buffer. Zero for every other conversion, including a` |
|       - |  467 | ``	 * `%s` — php passes appendstring `neg = false` there, which is why "%05s" of`` |
|       - |  468 | `	 * "-5" really is "000-5". */` |
|       - |  469 | `	char cLeadSign;` |
|       - |  470 | `	ph7_int64 iVal;` |
|       - |  471 | `	int precision;           /* Precision of the current field */` |
|       - |  472 | ``	/* php only has a precision when a DIGIT follows the '.' (its `expprec`): a bare`` |
|       - |  473 | `` 	 * "%.s" is a precision of zero that nothing consults, so `sprintf("%.s","abc")` `` |
|       - |  474 | ``	 * is "abc" and `sprintf("%.x",42)` is "2a". Reading the dot alone as an`` |
|       - |  475 | `	 * explicit zero truncated both to nothing. */` |
|       - |  476 | `	int bExplicitPrec;` |
|       - |  477 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|       - |  478 | `	int c,rc,n;` |
|   17077 |  479 | `	sxi32 rcRet = SXRET_OK;   /* Status to hand back through the single exit */` |
|   17077 |  480 | `	ph7_value *pThrowArg = 0; /* First not-stringable %s argument; throws at the end */` |
|       - |  481 | `	int length;              /* Length of the field */` |
|       - |  482 | `	int prefix;` |
|       - |  483 | `	sxu8 xtype;              /* Conversion paradigm */` |
|       - |  484 | `	int width;               /* Width of the current field */` |
|       - |  485 | `	int idx;` |
|   17077 |  486 | `	n = (vf == TRUE) ? 0 : 1;` |
|   17077 |  487 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|       - |  488 | `	/* Take the next argument as a scratch COPY: nothing below may write to the` |
|       - |  489 | `	 * caller's value. Answers 0 exactly as the raw form did when the arguments run` |
|       - |  490 | `	 * out (a shortfall is refused by PH7_FormatCheckArgCount before we get here).` |
|       - |  491 | `	 *` |
|       - |  492 | ``	 * A `%N$` specifier reads argument N and leaves the SEQUENTIAL cursor where it`` |
|       - |  493 | ``	 * was — php keeps the two apart (its `currarg` only ever advances for a`` |
|       - |  494 | `	 * specifier that carries no number), and the counting pass above has always` |
|       - |  495 | `	 * modelled it that way. The format loop did not: a positional MOVED the one` |
|       - |  496 | ``	 * cursor, so `sprintf('%1$s\|%s','a','b')` answered "a\|b" for php's "a\|a" and`` |
|       - |  497 | ``	 * `sprintf('%3$s\|%s','a','b','c')` ran off the end of the list it had just`` |
|       - |  498 | `	 * been told was long enough. */` |
|       - |  499 | `#define NEXT_ARG	( pRawArg = (nPos >= 0 ? (nPos < nArg ? apArg[nPos] : 0) \` |
|       - |  500 | `		: (n < nArg ? apArg[n++] : 0)), \` |
|       - |  501 | `	pRawArg ? (PH7_MemObjRelease(&sScratch), \` |
|       - |  502 | `		PH7_MemObjLoad(pRawArg,&sScratch), &sScratch) : 0 )` |
|       - |  503 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|       - |  504 | `	 * (called by every format builtin before this routine), so the specifier set` |
|       - |  505 | `	 * seen here is always valid. */` |
|       - |  506 | `	/* Start the format process */` |
|   32032 |  507 | `	for(;;){` |
|   65115 |  508 | `		zCur = zIn;` |
|  200949 |  509 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  135839 |  510 | `			zIn++;` |
|       5 |  511 | `		}` |
|   65115 |  512 | `		if( zCur < zIn ){` |
|       - |  513 | `			/* Consume chunk verbatim */` |
|   49981 |  514 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   49981 |  515 | `			if( rc != SXRET_OK ){` |
|       - |  516 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|     ! 0 |  517 | `				break;` |
|       - |  518 | `			}` |
|   24620 |  519 | `		}` |
|   65115 |  520 | `		if( zIn >= zEnd ){` |
|       - |  521 | `			/* No more input to process,break immediately */` |
|   17073 |  522 | `			break;` |
|       - |  523 | `		}` |
|       - |  524 | `		/* Find out what flags are present */` |
|   48047 |  525 | `		flag_leftjustify = flag_plussign =` |
|   48042 |  526 | `			flag_alternateform = flag_zeropad = 0;` |
|       - |  527 | `		/* Reset the pad character: a custom pad ('X) from a PREVIOUS specifier must` |
|       - |  528 | `		 * not bleed into this one. php resets it for every specifier. */` |
|   48047 |  529 | `		cPad = ' ';` |
|   48047 |  530 | `		bDropDigits = 0;` |
|   48047 |  531 | `		cLeadSign = 0;` |
|   48047 |  532 | `		nPos = -1;` |
|   48047 |  533 | `		zIn++; /* Jump the precent sign */` |
|   23668 |  534 | `		do{` |
|   69280 |  535 | `			c = zIn[0];` |
|   69280 |  536 | `			switch( c ){` |
|   20458 |  537 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     112 |  538 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      39 |  539 | `			case ' ':   cPad = ' ';               c = 0;   break;` |
|     590 |  540 | `			case '0':   cPad = '0';               c = 0;   break;` |
|      23 |  541 | `			case '\'':` |
|      47 |  542 | `				zIn++;` |
|      47 |  543 | `				if( zIn < zEnd ){` |
|       - |  544 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|      47 |  545 | `					cPad = zIn[0];` |
|      47 |  546 | `					c = 0;` |
|      23 |  547 | `				}` |
|      46 |  548 | `				break;` |
|   48042 |  549 | `			default:                                       break;` |
|       - |  550 | `			}` |
|   69280 |  551 | `		}while( c==0 && (zIn++ < zEnd) );` |
|       - |  552 | `		/* Get the field width (saturating — see FormatScanNumber) */` |
|   48047 |  553 | `		width = FormatScanNumber(&zIn,zEnd);` |
|   48047 |  554 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|       - |  555 | `			/* Position specifer */` |
|      48 |  556 | `			if( width > 0 ){` |
|      48 |  557 | `				nPos = vf ? width - 1 : width;` |
|      23 |  558 | `			}` |
|      48 |  559 | `			zIn++;` |
|      48 |  560 | `			width = 0;` |
|       - |  561 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|       - |  562 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|       - |  563 | `			 * not just zero-padding. */` |
|      23 |  564 | `			do{` |
|      50 |  565 | `				c = zIn[0];` |
|      50 |  566 | `				switch( c ){` |
|     ! 0 |  567 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     ! 0 |  568 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  569 | `				case ' ':   cPad = ' ';               c = 0;   break;` |
|     ! 0 |  570 | `				case '0':   cPad = '0';               c = 0;   break;` |
|       1 |  571 | `				case '\'':` |
|       3 |  572 | `					zIn++;` |
|       3 |  573 | `					if( zIn < zEnd ){` |
|       3 |  574 | `						cPad = zIn[0];` |
|       3 |  575 | `						c = 0;` |
|       1 |  576 | `					}` |
|       2 |  577 | `					break;` |
|      46 |  578 | `				default:                                       break;` |
|       - |  579 | `				}` |
|      50 |  580 | `			}while( c==0 && (zIn++ < zEnd) );` |
|      48 |  581 | `			width = FormatScanNumber(&zIn,zEnd);` |
|      23 |  582 | `		}` |
|       - |  583 | `		/* No clamp on the WIDTH: it used to be cut to the conversion buffer` |
|       - |  584 | `		 * (PH7_FMT_BUFSIZ-10 = 1014 bytes) because the zero padding was built` |
|       - |  585 | ``		 * INSIDE that buffer, so `sprintf("%%2000d",5)` answered 1014 characters and`` |
|       - |  586 | ``		 * `%%1100s` silently dropped 86 — a fixed-width record coming out short. The`` |
|       - |  587 | `		 * padding is emitted by the output block below, which chunks it and has no` |
|       - |  588 | `		 * such bound; the two zero-pad-into-zWorker sites are what needed the` |
|       - |  589 | `		 * limit, and both are gone (see cLeadSign). */` |
|       - |  590 | `		/* Get the precision */` |
|   48047 |  591 | `		precision = -1;` |
|   48047 |  592 | `		bExplicitPrec = 0;` |
|   48047 |  593 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     315 |  594 | `			zIn++;` |
|     315 |  595 | `			bExplicitPrec = ( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' );` |
|     315 |  596 | `			precision = FormatScanNumber(&zIn,zEnd);` |
|     156 |  597 | `		}` |
|       - |  598 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|       - |  599 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|       - |  600 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   48047 |  601 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|       9 |  602 | `			zIn++;` |
|       4 |  603 | `		}` |
|   48047 |  604 | `		if( zIn >= zEnd ){` |
|       - |  605 | `			/* No more input */` |
|     ! 0 |  606 | `			break;` |
|       - |  607 | `		}` |
|       - |  608 | `		/* Fetch the info entry for the field */` |
|   48047 |  609 | `		pInfo = 0;` |
|   48047 |  610 | `		xtype = PH7_FMT_ERROR;` |
|   48047 |  611 | `		c = zIn[0];` |
|   48047 |  612 | `		zIn++; /* Jump the format specifer */` |
|   96488 |  613 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   96488 |  614 | `			if( c==aFmt[idx].fmttype ){` |
|   48047 |  615 | `				pInfo = &aFmt[idx];` |
|   48047 |  616 | `				xtype = pInfo->type;` |
|   48047 |  617 | `				break;` |
|       - |  618 | `			}` |
|   23913 |  619 | `		}` |
|   48047 |  620 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   48047 |  621 | `		length = 0;` |
|       - |  622 | `		/* A '0' pad — however it was spelled, "%05d" or the custom "%'05d" — is` |
|       - |  623 | `		 * also php's "put the sign in front of the padding" rule` |
|       - |  624 | ``		 * (php_sprintf_appendstring's `(neg \|\| always_sign) && padding == '0'`),`` |
|       - |  625 | `		 * which is what the two zero-pad blocks below implement. */` |
|   48047 |  626 | `		flag_zeropad = (cPad == '0');` |
|       - |  627 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|       - |  628 | `		 /*` |
|       - |  629 | `		  ** At this point, variables are initialized as follows:` |
|       - |  630 | `		  **` |
|       - |  631 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|       - |  632 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|       - |  633 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|       - |  634 | `		  **                               field width was negative.` |
|       - |  635 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|       - |  636 | `		  **                               the conversion character.` |
|       - |  637 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|       - |  638 | `		  **   width                       The specified field width.  This is` |
|       - |  639 | `		  **                               always non-negative.  Zero is the default.` |
|       - |  640 | `		  **   precision                   The specified precision.  The default` |
|       - |  641 | `		  **                               is -1.` |
|       - |  642 | `		  */` |
|   48047 |  643 | `		switch(xtype){` |
|     102 |  644 | `		case PH7_FMT_PERCENT:` |
|       - |  645 | `			/* A literal percent character */` |
|     205 |  646 | `			zWorker[0] = '%';` |
|     205 |  647 | `			length = (int)sizeof(char);` |
|     205 |  648 | `			break;` |
|       5 |  649 | `		case PH7_FMT_CHARX:` |
|       - |  650 | `			/* The argument is treated as an integer, and presented as the character` |
|       - |  651 | `			 * with that ASCII value` |
|       - |  652 | `			 */` |
|      12 |  653 | `			pArg = NEXT_ARG;` |
|      12 |  654 | `			if( pArg == 0 ){` |
|     ! 0 |  655 | `				c = 0;` |
|     ! 0 |  656 | `			}else{` |
|       - |  657 | `				/* An integer conversion is a CAST site: php warns here for a float` |
|       - |  658 | `				 * no int can hold, then formats the wrapped value. */` |
|      12 |  659 | `				PH7_MemObjWarnIntCast(pArg);` |
|      12 |  660 | `				c = ph7_value_to_int(pArg);` |
|       - |  661 | `			}` |
|       - |  662 | `			/* NUL byte is an acceptable value */` |
|      12 |  663 | `			zWorker[0] = (char)c;` |
|      12 |  664 | `			length = (int)sizeof(char);` |
|       - |  665 | `			/* php's 'c' is the one conversion with no field: it appends the byte` |
|       - |  666 | `			 * through php_sprintf_appendchar, which takes neither a width nor an` |
|       - |  667 | `			 * alignment, so "%5c" and "%-5c" are both a bare one-byte string. */` |
|      12 |  668 | `			width = 0;` |
|      12 |  669 | `			break;` |
|   19502 |  670 | `		case PH7_FMT_STRING:` |
|       - |  671 | `			/* the argument is treated as and presented as a string */` |
|   38384 |  672 | `			pArg = NEXT_ARG;` |
|   38384 |  673 | `			if( pArg == 0 ){` |
|     ! 0 |  674 | `				length = 0;` |
|   38384 |  675 | `			}else if( PH7_MemObjIsNotStringable(pArg) ){` |
|       - |  676 | `				/* php's user-visible coercion for %s (§2), object half: a class with` |
|       - |  677 | `				 * no __toString() is the catchable "could not be converted to` |
|       - |  678 | `				 * string" Error — but php does NOT let it interrupt the format. The` |
|       - |  679 | `				 * conversion substitutes NOTHING, the format runs to the end, the` |
|       - |  680 | `				 * output is written, and only then does the Error surface. So the` |
|       - |  681 | `				 * throw cannot be RAISED here: PHL's VmThrowException runs an` |
|       - |  682 | `				 * in-place catch immediately, which would print the format's tail` |
|       - |  683 | `				 * after the catch body. Remember the value and throw once the` |
|       - |  684 | `				 * output is out (see the tail of this function). */` |
|      21 |  685 | `				zBuf = "";` |
|      21 |  686 | `				length = 0;` |
|      21 |  687 | `				if( pThrowArg == 0 ){` |
|       - |  688 | `					/* The CALLER's value, not the scratch copy: this one is used after` |
|       - |  689 | `					 * the loop, once the scratch has been reloaded (and released). */` |
|      19 |  690 | `					pThrowArg = pRawArg;` |
|       9 |  691 | `				}` |
|      11 |  692 | `			}else{` |
|       - |  693 | `				/* An ARRAY warns and renders as "Array"; a Stringable renders. */` |
|       - |  694 | `				const char *zSv;` |
|   38364 |  695 | `				sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zSv,&length);` |
|   38364 |  696 | `				zBuf = (char *)zSv;` |
|   38364 |  697 | `				if( rcSv != SXRET_OK ){` |
|       - |  698 | `					/* A __toString() that THREW: unlike the case above this one` |
|       - |  699 | `					 * cannot be predicted, and the throw has already run any` |
|       - |  700 | `					 * in-place catch. Stop formatting rather than emitting the` |
|       - |  701 | `					 * format's tail after the catch body — every other builtin that` |
|       - |  702 | `					 * calls user code (array_map, usort) stops the same way. php` |
|       - |  703 | `					 * keeps going and prints the tail; recorded divergence, and` |
|       - |  704 | `					 * both engines raise the same exception. */` |
|       3 |  705 | `					rcRet = rcSv;` |
|       3 |  706 | `					goto Done;` |
|       - |  707 | `				}` |
|       - |  708 | `			}` |
|   38382 |  709 | `			if( length < 1 ){` |
|       - |  710 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|       - |  711 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|       - |  712 | `				 * absent optional part gained a stray space. */` |
|     369 |  713 | `				zBuf = "";` |
|     369 |  714 | `				length = 0;` |
|     183 |  715 | `			}` |
|   38382 |  716 | `			if( bExplicitPrec && precision<length ){` |
|       7 |  717 | `				length = precision;` |
|       3 |  718 | `			}` |
|   38382 |  719 | `			break;` |
|    4575 |  720 | `		case PH7_FMT_RADIX: {` |
|       - |  721 | `			/* The digits are produced from an UNSIGNED accumulator. Two php rules` |
|       - |  722 | `			 * ride on that, and the inherited signed one got both wrong:` |
|       - |  723 | `			 *` |
|       - |  724 | `			 *  - only %d is SIGNED. %u/%x/%X/%o/%b REINTERPRET the same 64 bits as` |
|       - |  725 | `			 *    unsigned (php_sprintf_appenduint / php_sprintf_append2n cast to` |
|       - |  726 | `			 *    zend_ulong), so sprintf("%x",-1) is "ffffffffffffffff", not the` |
|       - |  727 | `			 *    magnitude "1" this used to print for every negative value;` |
|       - |  728 | `			 *  - the magnitude of PHP_INT_MIN has no signed representation, so` |
|       - |  729 | ``			 *    `iVal = -iVal` was signed overflow — undefined, and the guard`` |
|       - |  730 | ``			 *    testing for it afterwards (`if( iVal < 0 )`) is exactly what a`` |
|       - |  731 | `			 *    compiler may assume cannot happen. It did: the negative` |
|       - |  732 | ``			 *    accumulator reached `cset[iVal%base]`, indexing BEFORE the digit`` |
|       - |  733 | `			 *    table, so sprintf("%d",PHP_INT_MIN) printed whatever bytes sat` |
|       - |  734 | `			 *    there. Unsigned negation is well-defined for every input.` |
|       - |  735 | `			 */` |
|       - |  736 | `			sxu64 uVal;` |
|    9074 |  737 | `			pArg = NEXT_ARG;` |
|    9074 |  738 | `			if( pArg == 0 ){` |
|     ! 0 |  739 | `				iVal = 0;` |
|     ! 0 |  740 | `			}else{` |
|       - |  741 | ``				/* Every radix is a CAST site: `%d`/`%x`/`%u`/`%b`/`%o` of a float`` |
|       - |  742 | `				 * no int can hold warn, then print the modular wrap of it. */` |
|    9074 |  743 | `				PH7_MemObjWarnIntCast(pArg);` |
|    9074 |  744 | `				iVal = ph7_value_to_int64(pArg);` |
|       - |  745 | `			}` |
|       - |  746 | `			/* An integer conversion has no PRECISION in php: the '.' part of the` |
|       - |  747 | ``			 * specifier never reaches the digits. `%.5d` of 42 is "42", not the`` |
|       - |  748 | `			 * "00042" C would print — php's php_sprintf_appendint simply is not` |
|       - |  749 | `			 * handed one. For the other radices the same absence is louder:` |
|       - |  750 | `			 * php_sprintf_append2n forwards a max_width of 0 with the` |
|       - |  751 | `			 * "precision was given" flag set, so ANY explicit precision truncates` |
|       - |  752 | ``			 * the digits to nothing and `%.1x` of 42 is the EMPTY string (padded`` |
|       - |  753 | `			 * to $width, which is why "%5.1x" is five spaces). Reproduced rather` |
|       - |  754 | `			 * than smoothed over — parity is binding (§10). */` |
|    9074 |  755 | `			bDropDigits = (bExplicitPrec && pInfo->base != 10);` |
|    9074 |  756 | `			if( precision >= 0 ){` |
|      19 |  757 | `				precision = -1;` |
|       9 |  758 | `			}` |
|       - |  759 | `			/* php's "Can't right-pad 0's on integers" (php_sprintf_appendint, which` |
|       - |  760 | `			 * %u shares) — and only there: %x/%X/%o/%b go through append2n and %e/%f` |
|       - |  761 | `			 * through appenddouble, which both DO right-pad with zeros, so` |
|       - |  762 | `			 * "%-08x" of 5 really is "50000000" while "%-08d" is "5       ".` |
|       - |  763 | `			 * base 10 is exactly the 'd'/'u' pair of the table above. */` |
|    9074 |  764 | `			if( flag_leftjustify && flag_zeropad && pInfo->base == 10 ){` |
|      19 |  765 | `				flag_zeropad = 0;` |
|      19 |  766 | `				cPad = ' ';` |
|       9 |  767 | `			}` |
|       - |  768 | `        /* For the format %#x, the value zero is printed "0" not "0x0". */` |
|    9074 |  769 | `        if( iVal==0 ) flag_alternateform = 0;` |
|    9074 |  770 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    8378 |  771 | `          if( iVal<0 ){` |
|     464 |  772 | `            uVal = (sxu64)0 - (sxu64)iVal;` |
|     464 |  773 | `            prefix = '-';` |
|     234 |  774 | `          }else{` |
|    7918 |  775 | `            uVal = (sxu64)iVal;` |
|       - |  776 | `            /* php's ' ' is a PAD selector, not C's space-for-a-positive-sign, so` |
|       - |  777 | `             * '+' is the only flag that prefixes a non-negative value. */` |
|    7918 |  778 | `            prefix = flag_plussign ? '+' : 0;` |
|       - |  779 | `          }` |
|    4151 |  780 | `        }else{` |
|     700 |  781 | `			uVal = (sxu64)iVal;` |
|     700 |  782 | `			prefix = 0;` |
|       - |  783 | `		}` |
|       - |  784 | `        /* Zero padding is a RIGHT-aligned idea: it fills between the sign and the` |
|       - |  785 | `         * first digit. Left-aligned, php pads on the far side like any other pad` |
|       - |  786 | `         * character (append2n hands the '0' straight to appendstring's ALIGN_LEFT` |
|       - |  787 | `         * arm), so "%-08x" of 5 is "50000000". */` |
|    9074 |  788 | `        if( flag_zeropad && !flag_leftjustify ){` |
|     522 |  789 | `          cLeadSign = (char)prefix;` |
|     522 |  790 | `          prefix = 0;` |
|     259 |  791 | `        }` |
|    9074 |  792 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|       - |  793 | `        {` |
|       - |  794 | `          const char *cset;` |
|       - |  795 | `          sxu64 base;` |
|    9074 |  796 | `          cset = pInfo->charset;` |
|    9074 |  797 | `          base = (sxu64)pInfo->base;` |
|    4494 |  798 | `          do{                                           /* Convert to ascii */` |
|   20161 |  799 | `            *(--zBuf) = cset[uVal%base];` |
|   20161 |  800 | `            uVal = uVal/base;` |
|   20161 |  801 | `          }while( uVal>0 );` |
|       - |  802 | `        }` |
|    9074 |  803 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|       - |  804 | `        /* No zero fill here: a radix conversion has no precision to fill to, and` |
|       - |  805 | `         * the '0' pad is the output block's job now (see cLeadSign). */` |
|    9074 |  806 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    9074 |  807 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|       - |  808 | `          char *pre, x;` |
|     ! 0 |  809 | `          pre = pInfo->prefix;` |
|     ! 0 |  810 | `          if( *zBuf!=pre[0] ){` |
|     ! 0 |  811 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|     ! 0 |  812 | `          }` |
|     ! 0 |  813 | `        }` |
|    9074 |  814 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    9074 |  815 | `		if( bDropDigits ){` |
|       9 |  816 | `			length = 0;` |
|       4 |  817 | `		}` |
|    9074 |  818 | `		break;` |
|       - |  819 | `		}` |
|     190 |  820 | `		case PH7_FMT_FLOAT:` |
|       - |  821 | `		case PH7_FMT_EXP:` |
|       - |  822 | `		case PH7_FMT_GENERIC:{` |
|       - |  823 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - |  824 | `		double realvalue;` |
|       - |  825 | `		char zFmt[8];` |
|       - |  826 | `		int nOut, nFmt;` |
|     383 |  827 | `		pArg = NEXT_ARG;` |
|     383 |  828 | `		if( pArg == 0 ){` |
|     ! 0 |  829 | `			realvalue = 0;` |
|     ! 0 |  830 | `		}else{` |
|     383 |  831 | `			realvalue = ph7_value_to_double(pArg);` |
|       - |  832 | `		}` |
|       - |  833 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|       - |  834 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|     383 |  835 | `		if( PH7_IS_NAN(realvalue) ){` |
|      23 |  836 | `			zBuf = "NaN";` |
|      23 |  837 | `			length = 3;` |
|      23 |  838 | `			width = 0;` |
|      23 |  839 | `			break;` |
|       - |  840 | `		}` |
|     361 |  841 | `		if( PH7_IS_INF(realvalue) ){` |
|      37 |  842 | `			if( realvalue < 0.0 ){` |
|      15 |  843 | `				zBuf = "-INF";` |
|      15 |  844 | `				length = 4;` |
|       8 |  845 | `			}else{` |
|      23 |  846 | `				zBuf = "INF";` |
|      23 |  847 | `				length = 3;` |
|       - |  848 | `			}` |
|      37 |  849 | `			width = 0;` |
|      37 |  850 | `			break;` |
|       - |  851 | `		}` |
|     325 |  852 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|     325 |  853 | `		if( precision > 53 ){` |
|       - |  854 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|       - |  855 | `			 * (message prefixed with the active function's name, like` |
|       - |  856 | `			 * php_error_docref). */` |
|       - |  857 | `			char zMsg[160];` |
|       4 |  858 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - |  859 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|       2 |  860 | `				&pCtx->pFunc->sName,precision,53);` |
|       3 |  861 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|       3 |  862 | `			precision = 53;` |
|       1 |  863 | `		}` |
|       - |  864 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|       - |  865 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|     325 |  866 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|       9 |  867 | `			realvalue = 0.0;` |
|       4 |  868 | `		}` |
|       - |  869 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|       - |  870 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|       - |  871 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|       - |  872 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|       - |  873 | `		 * expansion), then post-process into php's exact shapes below. */` |
|     325 |  874 | `		nFmt = 0;` |
|     325 |  875 | `		zFmt[nFmt++] = '%';` |
|     325 |  876 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|       - |  877 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|       - |  878 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|     325 |  879 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|     325 |  880 | `		zFmt[nFmt++] = '.';` |
|     325 |  881 | `		zFmt[nFmt++] = '*';` |
|     539 |  882 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     115 |  883 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     198 |  884 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|     325 |  885 | `		zFmt[nFmt] = 0;` |
|     325 |  886 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|     325 |  887 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|       - |  888 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|       - |  889 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|     ! 0 |  890 | `			nOut = (int)SyStrlen(zWorker);` |
|     ! 0 |  891 | `		}` |
|     325 |  892 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|     325 |  893 | `		zBuf = zWorker;` |
|     325 |  894 | `		length = nOut;` |
|       - |  895 | `		/* The zero padding goes between the sign snprintf wrote and the first` |
|       - |  896 | `		 * digit, so hand the sign to the output block and leave the rest here. */` |
|     322 |  897 | `		if( flag_zeropad && !flag_leftjustify` |
|      23 |  898 | `		 && (zWorker[0]=='-' \|\| zWorker[0]=='+') ){` |
|      11 |  899 | `			cLeadSign = zWorker[0];` |
|      11 |  900 | `			zBuf++;` |
|      11 |  901 | `			length--;` |
|       5 |  902 | `		}` |
|       - |  903 | `#else` |
|       - |  904 | `         zBuf = " ";` |
|       - |  905 | `		 length = (int)sizeof(char);` |
|       - |  906 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     325 |  907 | `		 break;` |
|       - |  908 | `							 }` |
|     ! 0 |  909 | `		default:` |
|       - |  910 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|       - |  911 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|       - |  912 | `			 * no-op that emits nothing. */` |
|     ! 0 |  913 | `			length = 0;` |
|     ! 0 |  914 | `			break;` |
|       - |  915 | `		}` |
|       - |  916 | `		 /*` |
|       - |  917 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|       - |  918 | `		 ** "length" characters long.The field width is "width".Do` |
|       - |  919 | `		 ** the output.` |
|       - |  920 | `		 */` |
|   48045 |  921 | `    if( cLeadSign ){` |
|       - |  922 | `      /* php writes the sign ahead of a '0' pad run; it fills one byte of the` |
|       - |  923 | `       * field, so the padding below has that much less to do. */` |
|      37 |  924 | `      rc = xConsumer(pCtx,&cLeadSign,1,pUserData);` |
|      37 |  925 | `      if( rc != SXRET_OK ){` |
|     ! 0 |  926 | `        rcRet = SXERR_ABORT;` |
|     ! 0 |  927 | `        goto Done;` |
|       - |  928 | `      }` |
|      37 |  929 | `      width--;` |
|      18 |  930 | `    }` |
|   48045 |  931 | `    if( width > length ){` |
|       - |  932 | `      /* Fill the pad buffer with THIS specifier's pad character. */` |
| 1008020 |  933 | `      for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = cPad; }` |
|    9719 |  934 | `    }` |
|   48045 |  935 | `    if( !flag_leftjustify ){` |
|       - |  936 | `      register int nspace;` |
|   27592 |  937 | `      nspace = width-length;` |
|   27592 |  938 | `      if( nspace>0 ){` |
|    2945 |  939 | `        while( nspace>=etSPACESIZE ){` |
|    2103 |  940 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    2103 |  941 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  942 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  943 | `				goto Done;` |
|       - |  944 | `			}` |
|    2103 |  945 | `			nspace -= etSPACESIZE;` |
|       1 |  946 | `        }` |
|     843 |  947 | `        if( nspace>0 ){` |
|     843 |  948 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     843 |  949 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  950 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  951 | `				goto Done;` |
|       - |  952 | `			}` |
|     414 |  953 | `		}` |
|     414 |  954 | `      }` |
|   13600 |  955 | `    }` |
|   48045 |  956 | `    if( length>0 ){` |
|   47669 |  957 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   47669 |  958 | `		if( rc != SXRET_OK ){` |
|       3 |  959 | `		  rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       3 |  960 | `		  goto Done;` |
|       - |  961 | `		}` |
|   23478 |  962 | `    }` |
|   48043 |  963 | `    if( flag_leftjustify ){` |
|       - |  964 | `      register int nspace;` |
|   20458 |  965 | `      nspace = width-length;` |
|   20458 |  966 | `      if( nspace>0 ){` |
|   19440 |  967 | `        while( nspace>=etSPACESIZE ){` |
|     510 |  968 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|     510 |  969 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  970 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  971 | `				goto Done;` |
|       - |  972 | `			}` |
|     510 |  973 | `			nspace -= etSPACESIZE;` |
|       2 |  974 | `        }` |
|   18932 |  975 | `        if( nspace>0 ){` |
|   18932 |  976 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|   18932 |  977 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  978 | `				rcRet = SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     ! 0 |  979 | `				goto Done;` |
|       - |  980 | `			}` |
|    9305 |  981 | `		}` |
|    9305 |  982 | `      }` |
|   10067 |  983 | `    }` |
|       5 |  984 | ` }/* for(;;) */` |
|   17073 |  985 | `	if( pThrowArg ){` |
|       - |  986 | `		/* The format ran to completion and its output is out; raise php's Error` |
|       - |  987 | ``		 * now. `printf("A[%s]B", new P())` prints "A[]B" and THEN throws, while`` |
|       - |  988 | `		 * sprintf()'s finished result is simply discarded by the unwind. */` |
|      19 |  989 | `		PH7_MemObjRelease(&sScratch);` |
|      19 |  990 | `		return PH7_MemObjToStringUV(pThrowArg);` |
|       - |  991 | `	}` |
|    8695 |  992 | `Done:` |
|       - |  993 | `	/* Single exit: the scratch copy holds a reference on an array/instance` |
|       - |  994 | `	 * argument it was loaded from, so every way out releases it. */` |
|   17059 |  995 | `	PH7_MemObjRelease(&sScratch);` |
|   17059 |  996 | `	return rcRet;` |
|    8371 |  997 | `}` |
|       - |  998 | `/*` |
|       - |  999 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|       - | 1000 | ` */` |
|    9758 | 1001 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       5 | 1002 | `{` |
|       - | 1003 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|       - | 1004 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|       - | 1005 | `	 * non-OK rc also stops the format loop. */` |
|    9763 | 1006 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    9763 | 1007 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    9763 | 1008 | `	return *pRc;` |
|       5 | 1009 | `}` |
|       - | 1010 | `/*` |
|       - | 1011 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|       - | 1012 | ` *  Return a formatted string.` |
|       - | 1013 | ` * Parameters` |
|       - | 1014 | ` *  $format` |
|       - | 1015 | ` *    The format string (see block comment above)` |
|       - | 1016 | ` * Return` |
|       - | 1017 | ` *  A string produced according to the formatting string format.` |
|       - | 1018 | ` */` |
|    1712 | 1019 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1020 | `{` |
|       - | 1021 | `	sxi32 rcFmt;` |
|       - | 1022 | `	const char *zFormat;` |
|    1717 | 1023 | `	sxi32 rc = SXRET_OK;` |
|       - | 1024 | `	int nLen;` |
|    1717 | 1025 | `	if( nArg < 1 ){` |
|       - | 1026 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 1027 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1028 | `		return PH7_OK;` |
|       - | 1029 | `	}` |
|       - | 1030 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    1717 | 1031 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    1717 | 1032 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1033 | `		return rc;` |
|       - | 1034 | `	}` |
|       - | 1035 | `	/* Extract the string format (scalars/null coerce). */` |
|    1717 | 1036 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    1717 | 1037 | `	if( nLen < 1 ){` |
|       - | 1038 | `		/* Empty string */` |
|     ! 0 | 1039 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1040 | `		return PH7_OK;` |
|       - | 1041 | `	}` |
|       - | 1042 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|       - | 1043 | `	 * output; propagate the throw status verbatim. */` |
|    1717 | 1044 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    1717 | 1045 | `	if( rc != PH7_OK ){` |
|      41 | 1046 | `		return rc;` |
|       - | 1047 | `	}` |
|       - | 1048 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    1677 | 1049 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    1677 | 1050 | `	if( rc != PH7_OK ){` |
|      31 | 1051 | `		return rc;` |
|       - | 1052 | `	}` |
|       - | 1053 | `	/* Seed the result with the empty string: a format whose every conversion` |
|       - | 1054 | `	 * substitutes NOTHING ("%s" of "", false or null) never calls the consumer at` |
|       - | 1055 | `	 * all, and an untouched return value is NULL — so sprintf("%s","") answered` |
|       - | 1056 | `	 * NULL where php answers "". */` |
|    1647 | 1057 | `	ph7_result_string(pCtx,"",0);` |
|       - | 1058 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    1647 | 1059 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    1647 | 1060 | `	if( rc != SXRET_OK ){` |
|       - | 1061 | `		/* The result append ran out of memory: raise a fatal rather than` |
|       - | 1062 | `		 * returning a silently-truncated string. */` |
|     ! 0 | 1063 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1064 | `	}` |
|       - | 1065 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1066 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1067 | `	 * that), so report the throw last. */` |
|    1647 | 1068 | `	if( rcFmt != SXRET_OK ){` |
|      12 | 1069 | `		pCtx->nThrowRc = rcFmt;` |
|      12 | 1070 | `		return rcFmt;` |
|       - | 1071 | `	}` |
|    1637 | 1072 | `	return PH7_OK;` |
|     858 | 1073 | `}` |
|       - | 1074 | `/*` |
|       - | 1075 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|       - | 1076 | ` */` |
|  110235 | 1077 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|       5 | 1078 | `{` |
|  110240 | 1079 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|       - | 1080 | `	/* Call the VM output consumer directly */` |
|  110240 | 1081 | `	ph7_context_output(pCtx,zInput,nLen);` |
|       - | 1082 | `	/* Increment counter */` |
|  110240 | 1083 | `	*pCounter += nLen;` |
|  110240 | 1084 | `	return PH7_OK;` |
|       5 | 1085 | `}` |
|       - | 1086 | `/*` |
|       - | 1087 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|       - | 1088 | ` *  Output a formatted string.` |
|       - | 1089 | ` * Parameters` |
|       - | 1090 | ` *  $format` |
|       - | 1091 | ` *   See sprintf() for a description of format.` |
|       - | 1092 | ` * Return` |
|       - | 1093 | ` *  The length of the outputted string.` |
|       - | 1094 | ` */` |
|   15384 | 1095 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1096 | `{` |
|       - | 1097 | `	sxi32 rcFmt;` |
|   15389 | 1098 | `	ph7_int64 nCounter = 0;` |
|       - | 1099 | `	const char *zFormat;` |
|       - | 1100 | `	int nLen;` |
|   15389 | 1101 | `	if( nArg < 1 ){` |
|       - | 1102 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 1103 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1104 | `		return PH7_OK;` |
|       - | 1105 | `	}` |
|       - | 1106 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|       - | 1107 | `	{` |
|   15389 | 1108 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   15389 | 1109 | `		if( rcf != PH7_OK ){` |
|     ! 0 | 1110 | `			return rcf;` |
|       - | 1111 | `		}` |
|       - | 1112 | `	}` |
|       - | 1113 | `	/* Extract the string format (scalars/null coerce). */` |
|   15389 | 1114 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   15389 | 1115 | `	if( nLen < 1 ){` |
|       - | 1116 | `		/* Empty string */` |
|     ! 0 | 1117 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1118 | `		return PH7_OK;` |
|       - | 1119 | `	}` |
|       - | 1120 | `	{` |
|       - | 1121 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|       - | 1122 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|   15389 | 1123 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   15389 | 1124 | `		if( rcv != PH7_OK ){` |
|       3 | 1125 | `			return rcv;` |
|       - | 1126 | `		}` |
|       - | 1127 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|       - | 1128 | `		 * before any output; propagate the throw status verbatim. */` |
|   15387 | 1129 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   15387 | 1130 | `		if( rcv != PH7_OK ){` |
|     ! 0 | 1131 | `			return rcv;` |
|       - | 1132 | `		}` |
|       - | 1133 | `	}` |
|       - | 1134 | `	/* Format the string */` |
|   15387 | 1135 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|       - | 1136 | `	/* Return the length of the outputted string */` |
|   15387 | 1137 | `	ph7_result_int64(pCtx,nCounter);` |
|       - | 1138 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1139 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1140 | `	 * that), so report the throw last. */` |
|   15387 | 1141 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1142 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1143 | `		return rcFmt;` |
|       - | 1144 | `	}` |
|   15385 | 1145 | `	return PH7_OK;` |
|    7530 | 1146 | `}` |
|       - | 1147 | `/*` |
|       - | 1148 | ` * int vprintf(string $format,array $args)` |
|       - | 1149 | ` *  Output a formatted string.` |
|       - | 1150 | ` * Parameters` |
|       - | 1151 | ` *  $format` |
|       - | 1152 | ` *   See sprintf() for a description of format.` |
|       - | 1153 | ` * Return` |
|       - | 1154 | ` *  The length of the outputted string.` |
|       - | 1155 | ` */` |
|       6 | 1156 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1157 | `{` |
|       - | 1158 | `	sxi32 rcFmt;` |
|       8 | 1159 | `	ph7_int64 nCounter = 0;` |
|       - | 1160 | `	const char *zFormat;` |
|       - | 1161 | `	ph7_hashmap *pMap;` |
|       - | 1162 | `	SySet sArg;` |
|       - | 1163 | `	int nLen,n;` |
|       8 | 1164 | `	if( nArg < 2 ){` |
|       - | 1165 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 1166 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1167 | `		return PH7_OK;` |
|       - | 1168 | `	}` |
|       - | 1169 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|       8 | 1170 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|       8 | 1171 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1172 | `		return rcFmt;` |
|       - | 1173 | `	}` |
|       8 | 1174 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1175 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - | 1176 | `		char zBuf[64];` |
|     ! 0 | 1177 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1178 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 1179 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 1180 | `	}` |
|       - | 1181 | `	/* Extract the string format (scalars/null coerce). */` |
|       8 | 1182 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|       8 | 1183 | `	if( nLen < 1 ){` |
|       - | 1184 | `		/* Empty string */` |
|     ! 0 | 1185 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1186 | `		return PH7_OK;` |
|       - | 1187 | `	}` |
|       - | 1188 | `	/* Point to the hashmap */` |
|       8 | 1189 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1190 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - | 1191 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|       - | 1192 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|       - | 1193 | `	 * rather than the missing specifier. */` |
|       8 | 1194 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|       8 | 1195 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1196 | `		return rcFmt;` |
|       - | 1197 | `	}` |
|       - | 1198 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - | 1199 | `	 * any output; propagate the throw status verbatim. */` |
|       8 | 1200 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|       8 | 1201 | `	if( rcFmt != PH7_OK ){` |
|     ! 0 | 1202 | `		return rcFmt;` |
|       - | 1203 | `	}` |
|       - | 1204 | `	/* Extract arguments from the hashmap */` |
|       8 | 1205 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - | 1206 | `	/* Format the string */` |
|       8 | 1207 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|       - | 1208 | `	/* Release the container */` |
|       8 | 1209 | `	SySetRelease(&sArg);` |
|       - | 1210 | `	/* Return the length of the outputted string */` |
|       8 | 1211 | `	ph7_result_int64(pCtx,nCounter);` |
|       - | 1212 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1213 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1214 | `	 * that), so report the throw last. */` |
|       8 | 1215 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1216 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1217 | `		return rcFmt;` |
|       - | 1218 | `	}` |
|       5 | 1219 | `	return PH7_OK;` |
|       5 | 1220 | `}` |
|       - | 1221 | `/*` |
|       - | 1222 | ` * int vsprintf(string $format,array $args)` |
|       - | 1223 | ` *  Output a formatted string.` |
|       - | 1224 | ` * Parameters` |
|       - | 1225 | ` *  $format` |
|       - | 1226 | ` *   See sprintf() for a description of format.` |
|       - | 1227 | ` * Return` |
|       - | 1228 | ` *  A string produced according to the formatting string format.` |
|       - | 1229 | ` */` |
|      24 | 1230 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1231 | `{` |
|       - | 1232 | `	sxi32 rcFmt;` |
|       - | 1233 | `	const char *zFormat;` |
|       - | 1234 | `	ph7_hashmap *pMap;` |
|       - | 1235 | `	SySet sArg;` |
|      26 | 1236 | `	sxi32 rc = SXRET_OK;` |
|       - | 1237 | `	int nLen,n;` |
|      26 | 1238 | `	if( nArg < 2 ){` |
|       - | 1239 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 1240 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1241 | `		return PH7_OK;` |
|       - | 1242 | `	}` |
|       - | 1243 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      26 | 1244 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      26 | 1245 | `	if( rc != PH7_OK ){` |
|     ! 0 | 1246 | `		return rc;` |
|       - | 1247 | `	}` |
|      26 | 1248 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1249 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|       - | 1250 | `		char zBuf[64];` |
|     ! 0 | 1251 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1252 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     ! 0 | 1253 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 1254 | `	}` |
|       - | 1255 | `	/* Extract the string format (scalars/null coerce). */` |
|      26 | 1256 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      26 | 1257 | `	if( nLen < 1 ){` |
|       - | 1258 | `		/* Empty string */` |
|     ! 0 | 1259 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1260 | `		return PH7_OK;` |
|       - | 1261 | `	}` |
|       - | 1262 | `	/* Point to hashmap */` |
|      26 | 1263 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 1264 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|       - | 1265 | `	 * php runs this BEFORE validating the specifiers. */` |
|      26 | 1266 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      26 | 1267 | `	if( rcFmt != PH7_OK ){` |
|       7 | 1268 | `		return rcFmt;` |
|       - | 1269 | `	}` |
|       - | 1270 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|       - | 1271 | `	 * any output; propagate the throw status verbatim. */` |
|      20 | 1272 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      20 | 1273 | `	if( rcFmt != PH7_OK ){` |
|       3 | 1274 | `		return rcFmt;` |
|       - | 1275 | `	}` |
|       - | 1276 | `	/* Extract arguments from the hashmap */` |
|      18 | 1277 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|       - | 1278 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|       - | 1279 | `	/* Empty-result seed — see PH7_builtin_sprintf. */` |
|      18 | 1280 | `	ph7_result_string(pCtx,"",0);` |
|      18 | 1281 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|       - | 1282 | `	/* Release the container */` |
|      18 | 1283 | `	SySetRelease(&sArg);` |
|      18 | 1284 | `	if( rc != SXRET_OK ){` |
|       - | 1285 | `		/* The result append ran out of memory: raise a fatal. */` |
|     ! 0 | 1286 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1287 | `	}` |
|       - | 1288 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|       - | 1289 | `	 * format still ran and the output/result still happened (php does exactly` |
|       - | 1290 | `	 * that), so report the throw last. */` |
|      18 | 1291 | `	if( rcFmt != SXRET_OK ){` |
|       3 | 1292 | `		pCtx->nThrowRc = rcFmt;` |
|       3 | 1293 | `		return rcFmt;` |
|       - | 1294 | `	}` |
|      15 | 1295 | `	return PH7_OK;` |
|      14 | 1296 | `}` |
|       - | 1297 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|       - | 1298 |  |
