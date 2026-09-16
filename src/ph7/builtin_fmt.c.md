# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 482/565 lines (85.31%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|     - |    8 | `                      * rounded shortest-representation output) */` |
|     - |    9 | `/*` |
|     - |   10 | ` * Section:` |
|     - |   11 | ` *    printf-style format engine and the sprintf/printf function family.` |
|     - |   12 | ` * Status:` |
|     - |   13 | ` *    Stable.` |
|     - |   14 | ` */` |
|     - |   15 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |   16 | `#define PH7_NEED_FMT_AND_INI 1` |
|     - |   17 | `#endif` |
|     - |   18 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - |   19 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|     - |   20 | `/*` |
|     - |   21 | `** Conversion types fall into various categories as defined by the` |
|     - |   22 | `** following enumeration.` |
|     - |   23 | `*/` |
|     - |   24 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|     - |   25 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|     - |   26 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|     - |   27 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|     - |   28 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|     - |   29 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|     - |   30 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|     - |   31 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|     - |   32 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|     - |   33 |  |
|     - |   34 | `/*` |
|     - |   35 | `** Allowed values for ph7_fmt_info.flags` |
|     - |   36 | `*/` |
|     - |   37 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|     - |   38 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|     - |   39 | `/*` |
|     - |   40 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|     - |   41 | `** by an instance of the following structure` |
|     - |   42 | `*/` |
|     - |   43 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|     - |   44 | `struct ph7_fmt_info` |
|     - |   45 | `{` |
|     - |   46 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|     - |   47 | `  sxu8 base;     /* The base for radix conversion */` |
|     - |   48 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|     - |   49 | `  sxu8 type;     /* Conversion paradigm */` |
|     - |   50 | `  char *charset; /* The character set for conversion */` |
|     - |   51 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|     - |   52 | `};` |
|     - |   53 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|     - |   54 | ` * the default float->string cast needs it even when this whole formatting` |
|     - |   55 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|     - |   56 | `/*` |
|     - |   57 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|     - |   58 | ` * used conversion types first.` |
|     - |   59 | ` */` |
|     - |   60 | `static const ph7_fmt_info aFmt[] = {` |
|     - |   61 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|     - |   62 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|     - |   63 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|     - |   64 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|     - |   65 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|     - |   66 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|     - |   67 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|     - |   68 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|     - |   69 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|     - |   70 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|     - |   71 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|     - |   72 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|     - |   73 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|     - |   74 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|     - |   75 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|     - |   76 | `   * formats in the C locale, so they behave identically. */` |
|     - |   77 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|     - |   78 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|     - |   79 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|     - |   80 | `};` |
|     - |   81 | `/*` |
|     - |   82 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|     - |   83 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|     - |   84 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|     - |   85 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|     - |   86 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|     - |   87 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|     - |   88 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|     - |   89 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|     - |   90 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|     - |   91 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|     - |   92 | ` * "all valid".)` |
|     - |   93 | ` */` |
|   498 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|     3 |   95 | `{` |
|   501 |   96 | `	const char *zEnd = &zIn[nByte];` |
|     - |   97 | `	int c,idx;` |
|  3949 |   98 | `	while( zIn < zEnd ){` |
|  3485 |   99 | `		if( zIn[0] != '%' ){` |
|  2525 |  100 | `			zIn++;` |
|  2525 |  101 | `			continue;` |
|     - |  102 | `		}` |
|   961 |  103 | `		zIn++; /* jump the percent sign */` |
|     - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|     - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|     - |  106 | `		 * unknown specifier, matching php. */` |
|  1203 |  107 | `		while( zIn < zEnd ){` |
|  1193 |  108 | `			c = zIn[0];` |
|  1193 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   231 |  110 | `				zIn++;` |
|   231 |  111 | `				continue;` |
|     - |  112 | `			}` |
|   963 |  113 | `			if( c=='\'' ){` |
|    13 |  114 | `				zIn++;` |
|    13 |  115 | `				if( zIn < zEnd ){` |
|    13 |  116 | `					zIn++; /* the custom pad character */` |
|     6 |  117 | `				}` |
|    13 |  118 | `				continue;` |
|     - |  119 | `			}` |
|   951 |  120 | `			break;` |
|   ! 0 |  121 | `		}` |
|     - |  122 | `		/* field width */` |
|  1273 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   313 |  124 | `			zIn++;` |
|     1 |  125 | `		}` |
|     - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|     - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   961 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    11 |  129 | `			zIn++;` |
|    13 |  130 | `			while( zIn < zEnd ){` |
|    11 |  131 | `				c = zIn[0];` |
|    11 |  132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   ! 0 |  133 | `					zIn++;` |
|   ! 0 |  134 | `					continue;` |
|     - |  135 | `				}` |
|    11 |  136 | `				if( c=='\'' ){` |
|     3 |  137 | `					zIn++;` |
|     3 |  138 | `					if( zIn < zEnd ){` |
|     3 |  139 | `						zIn++;` |
|     1 |  140 | `					}` |
|     3 |  141 | `					continue;` |
|     - |  142 | `				}` |
|     9 |  143 | `				break;` |
|   ! 0 |  144 | `			}` |
|    19 |  145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     9 |  146 | `				zIn++;` |
|     1 |  147 | `			}` |
|     5 |  148 | `		}` |
|     - |  149 | `		/* precision */` |
|   961 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   113 |  151 | `			zIn++;` |
|   243 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   133 |  153 | `				zIn++;` |
|     3 |  154 | `			}` |
|    55 |  155 | `		}` |
|     - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   961 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|    11 |  158 | `			zIn++;` |
|     5 |  159 | `		}` |
|   961 |  160 | `		if( zIn >= zEnd ){` |
|     - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|     - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|    15 |  163 | `			*pbDangling = TRUE;` |
|    15 |  164 | `			return FALSE;` |
|     - |  165 | `		}` |
|   947 |  166 | `		c = zIn[0];` |
|   947 |  167 | `		zIn++; /* jump the conversion specifier */` |
|  3847 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|  3827 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   927 |  170 | `				break;` |
|     - |  171 | `			}` |
|  1453 |  172 | `		}` |
|   947 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|    21 |  174 | `			*pBad = c; /* unknown specifier */` |
|    21 |  175 | `			return TRUE;` |
|     - |  176 | `		}` |
|     3 |  177 | `	}` |
|   467 |  178 | `	return FALSE;` |
|   252 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|     - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|     - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|     - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|     - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|     - |  186 | ` * Returns PH7_OK when the format is valid.` |
|     - |  187 | ` */` |
|   498 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|     3 |  189 | `{` |
|   501 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   501 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|    31 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    10 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|     - |  194 | `	}` |
|   481 |  195 | `	if( bDangling ){` |
|    15 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  197 | `			"Missing format specifier at end of string");` |
|     - |  198 | `	}` |
|   467 |  199 | `	return PH7_OK;` |
|   252 |  200 | `}` |
|     - |  201 | `/*` |
|     - |  202 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|     - |  203 | ` * sequential (non-positional) conversion count and the highest positional index` |
|     - |  204 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|     - |  205 | ` */` |
|   526 |  206 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|     3 |  207 | `{` |
|   529 |  208 | `	const char *zEnd = &zIn[nByte];` |
|   529 |  209 | `	int c,seq = 0,maxpos = 0;` |
|  4073 |  210 | `	while( zIn < zEnd ){` |
|  3567 |  211 | `		int numVal = 0,pos = 0;` |
|  3567 |  212 | `		if( zIn[0] != '%' ){` |
|  2563 |  213 | `			zIn++;` |
|  2563 |  214 | `			continue;` |
|     - |  215 | `		}` |
|  1005 |  216 | `		zIn++; /* jump the percent sign */` |
|     - |  217 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|  1247 |  218 | `		while( zIn < zEnd ){` |
|  1231 |  219 | `			c = zIn[0];` |
|  1231 |  220 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|  1001 |  221 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|   989 |  222 | `			break;` |
|   ! 0 |  223 | `		}` |
|     - |  224 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|  1323 |  225 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   319 |  226 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|   319 |  227 | `			zIn++;` |
|     1 |  228 | `		}` |
|  1005 |  229 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    17 |  230 | `			pos = numVal;` |
|    17 |  231 | `			zIn++;` |
|     - |  232 | `			/* flags then width may follow the positional marker */` |
|    19 |  233 | `			while( zIn < zEnd ){` |
|    17 |  234 | `				c = zIn[0];` |
|    17 |  235 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    17 |  236 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    15 |  237 | `				break;` |
|   ! 0 |  238 | `			}` |
|    25 |  239 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     8 |  240 | `		}` |
|     - |  241 | `		/* precision */` |
|  1005 |  242 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   113 |  243 | `			zIn++;` |
|   243 |  244 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|    55 |  245 | `		}` |
|     - |  246 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|  1005 |  247 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|  1005 |  248 | `		if( zIn >= zEnd ){` |
|     - |  249 | `			/* A dangling '%' still COUNTS as needing a value: php reports` |
|     - |  250 | `			 * sprintf("%") as "2 arguments are required, 1 given" and only` |
|     - |  251 | `			 * raises the missing-specifier ValueError once the count is met. */` |
|    21 |  252 | `			if( pos > 0 ){` |
|     3 |  253 | `				if( pos > maxpos ){ maxpos = pos; }` |
|     2 |  254 | `			}else{` |
|    19 |  255 | `				seq++;` |
|     - |  256 | `			}` |
|    21 |  257 | `			break;` |
|     - |  258 | `		}` |
|   985 |  259 | `		c = zIn[0];` |
|   985 |  260 | `		zIn++; /* jump the conversion specifier */` |
|   985 |  261 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   975 |  262 | `		if( pos > 0 ){` |
|    15 |  263 | `			if( pos > maxpos ){ maxpos = pos; }` |
|     8 |  264 | `		}else{` |
|   961 |  265 | `			seq++;` |
|     - |  266 | `		}` |
|     3 |  267 | `	}` |
|   529 |  268 | `	return seq > maxpos ? seq : maxpos;` |
|     3 |  269 | `}` |
|     - |  270 | `/*` |
|     - |  271 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|     - |  272 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|     - |  273 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|     - |  274 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|     - |  275 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|     - |  276 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|     - |  277 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|     - |  278 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|     - |  279 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|     - |  280 | ` * when enough.` |
|     - |  281 | ` */` |
|   526 |  282 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|     3 |  283 | `{` |
|   529 |  284 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|   529 |  285 | `	if( nValues < required ){` |
|    29 |  286 | `		if( bVararg ){` |
|    13 |  287 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 |  288 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|     - |  289 | `		}` |
|    31 |  290 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    10 |  291 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|     - |  292 | `	}` |
|   501 |  293 | `	return PH7_OK;` |
|   266 |  294 | `}` |
|     - |  295 | `/*` |
|     - |  296 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|     - |  297 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|     - |  298 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|     - |  299 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|     - |  300 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|     - |  301 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|     - |  302 | ` */` |
|     - |  303 | `/*` |
|     - |  304 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|     - |  305 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|     - |  306 | ` */` |
|    24 |  307 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|     1 |  308 | `{` |
|    25 |  309 | `	if( !ph7_value_is_resource(pArg) ){` |
|     - |  310 | `		char zBuf[64];` |
|     4 |  311 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  312 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|     1 |  313 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|     - |  314 | `	}` |
|    23 |  315 | `	return PH7_OK;` |
|    13 |  316 | `}` |
|   536 |  317 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|     3 |  318 | `{` |
|   539 |  319 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|     - |  320 | `		char zBuf[64];` |
|   ! 0 |  321 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  322 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|   ! 0 |  323 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|     - |  324 | `	}` |
|   539 |  325 | `	return PH7_OK;` |
|   271 |  326 | `}` |
|     - |  327 | `/*` |
|     - |  328 | ` * Format a given string.` |
|     - |  329 | ` * The root program.  All variations call this core.` |
|     - |  330 | ` * INPUTS:` |
|     - |  331 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|     - |  332 | ` *            1. A pointer to the call context.` |
|     - |  333 | ` *            2. A pointer to the list of characters to be output` |
|     - |  334 | ` *               (Note, this list is NOT null terminated.)` |
|     - |  335 | ` *            3. An integer number of characters to be output.` |
|     - |  336 | ` *               (Note: This number might be zero.)` |
|     - |  337 | ` *            4. Upper layer private data.` |
|     - |  338 | ` *   zIn       This is the format string, as in the usual print.` |
|     - |  339 | ` *   apArg     This is a pointer to a list of arguments.` |
|     - |  340 | ` */` |
|   464 |  341 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|     - |  342 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|     - |  343 | `	ph7_context *pCtx,  /* call context */` |
|     - |  344 | `	const char *zIn,    /* Format string */` |
|     - |  345 | `	int nByte,          /* Format string length */` |
|     - |  346 | `	int nArg,           /* Total argument of the given arguments */` |
|     - |  347 | `	ph7_value **apArg,  /* User arguments */` |
|     - |  348 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|     - |  349 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|     - |  350 | `	)` |
|     3 |  351 | `{` |
|   467 |  352 | `	char spaces[] = "                                                  ";` |
|     - |  353 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   467 |  354 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - |  355 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|     - |  356 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|     - |  357 | `	int flag_alternateform; /* True if "#" flag is present */` |
|     - |  358 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|     - |  359 | `	int flag_blanksign;     /* True if " " flag is present */` |
|     - |  360 | `	int flag_plussign;      /* True if "+" flag is present */` |
|     - |  361 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|     - |  362 | `	ph7_value *pArg;         /* Current processed argument */` |
|     - |  363 | `	ph7_int64 iVal;` |
|     - |  364 | `	int precision;           /* Precision of the current field */` |
|     - |  365 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|     - |  366 | `	int c,rc,n;` |
|     - |  367 | `	int length;              /* Length of the field */` |
|     - |  368 | `	int prefix;` |
|     - |  369 | `	sxu8 xtype;              /* Conversion paradigm */` |
|     - |  370 | `	int width;               /* Width of the current field */` |
|     - |  371 | `	int idx;` |
|   467 |  372 | `	n = (vf == TRUE) ? 0 : 1;` |
|     - |  373 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|     - |  374 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|     - |  375 | `	 * (called by every format builtin before this routine), so the specifier set` |
|     - |  376 | `	 * seen here is always valid. */` |
|     - |  377 | `	/* Start the format process */` |
|   693 |  378 | `	for(;;){` |
|  1389 |  379 | `		zCur = zIn;` |
|  3887 |  380 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  2499 |  381 | `			zIn++;` |
|     1 |  382 | `		}` |
|  1389 |  383 | `		if( zCur < zIn ){` |
|     - |  384 | `			/* Consume chunk verbatim */` |
|   813 |  385 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   813 |  386 | `			if( rc != SXRET_OK ){` |
|     - |  387 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|   ! 0 |  388 | `				break;` |
|     - |  389 | `			}` |
|   406 |  390 | `		}` |
|  1389 |  391 | `		if( zIn >= zEnd ){` |
|     - |  392 | `			/* No more input to process,break immediately */` |
|   467 |  393 | `			break;` |
|     - |  394 | `		}` |
|     - |  395 | `		/* Find out what flags are present */` |
|   925 |  396 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|   922 |  397 | `			flag_alternateform = flag_zeropad = 0;` |
|     - |  398 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|     - |  399 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|     - |  400 | `		 * php resets the pad character for every specifier. */` |
| 47025 |  401 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|   925 |  402 | `		zIn++; /* Jump the precent sign */` |
|   461 |  403 | `		do{` |
|  1165 |  404 | `			c = zIn[0];` |
|  1165 |  405 | `			switch( c ){` |
|    19 |  406 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     7 |  407 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|     7 |  408 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|   199 |  409 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|     6 |  410 | `			case '\'':` |
|    13 |  411 | `				zIn++;` |
|    13 |  412 | `				if( zIn < zEnd ){` |
|     - |  413 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    13 |  414 | `					c = zIn[0];` |
|   613 |  415 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   601 |  416 | `						spaces[idx] = (char)c;` |
|   301 |  417 | `					}` |
|    13 |  418 | `					c = 0;` |
|     6 |  419 | `				}` |
|    12 |  420 | `				break;` |
|   922 |  421 | `			default:                                       break;` |
|     - |  422 | `			}` |
|  1165 |  423 | `		}while( c==0 && (zIn++ < zEnd) );` |
|     - |  424 | `		/* Get the field width */` |
|   925 |  425 | `		width = 0;` |
|  1694 |  426 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   309 |  427 | `			width = width*10 + (zIn[0] - '0');` |
|   309 |  428 | `			zIn++;` |
|     1 |  429 | `		}` |
|   925 |  430 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|     - |  431 | `			/* Position specifer */` |
|     9 |  432 | `			if( width > 0 ){` |
|     9 |  433 | `				n = width;` |
|     9 |  434 | `				if( vf && n > 0 ){` |
|   ! 0 |  435 | `					n--;` |
|   ! 0 |  436 | `				}` |
|     4 |  437 | `			}` |
|     9 |  438 | `			zIn++;` |
|     9 |  439 | `			width = 0;` |
|     - |  440 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|     - |  441 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|     - |  442 | `			 * not just zero-padding. */` |
|     4 |  443 | `			do{` |
|    11 |  444 | `				c = zIn[0];` |
|    11 |  445 | `				switch( c ){` |
|   ! 0 |  446 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|   ! 0 |  447 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|   ! 0 |  448 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|   ! 0 |  449 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|     1 |  450 | `				case '\'':` |
|     3 |  451 | `					zIn++;` |
|     3 |  452 | `					if( zIn < zEnd ){` |
|     3 |  453 | `						c = zIn[0];` |
|   103 |  454 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   101 |  455 | `							spaces[idx] = (char)c;` |
|    51 |  456 | `						}` |
|     3 |  457 | `						c = 0;` |
|     1 |  458 | `					}` |
|     2 |  459 | `					break;` |
|     8 |  460 | `				default:                                       break;` |
|     - |  461 | `				}` |
|    11 |  462 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    21 |  463 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     9 |  464 | `				width = width*10 + (zIn[0] - '0');` |
|     9 |  465 | `				zIn++;` |
|     1 |  466 | `			}` |
|     4 |  467 | `		}` |
|   925 |  468 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|   ! 0 |  469 | `			width = PH7_FMT_BUFSIZ-10;` |
|   ! 0 |  470 | `		}` |
|     - |  471 | `		/* Get the precision */` |
|   925 |  472 | `		precision = -1;` |
|   925 |  473 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|   113 |  474 | `			precision = 0;` |
|   113 |  475 | `			zIn++;` |
|   298 |  476 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   133 |  477 | `				precision = precision*10 + (zIn[0] - '0');` |
|   133 |  478 | `				zIn++;` |
|     3 |  479 | `			}` |
|    55 |  480 | `		}` |
|     - |  481 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|     - |  482 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|     - |  483 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   925 |  484 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|     9 |  485 | `			zIn++;` |
|     4 |  486 | `		}` |
|   925 |  487 | `		if( zIn >= zEnd ){` |
|     - |  488 | `			/* No more input */` |
|   ! 0 |  489 | `			break;` |
|     - |  490 | `		}` |
|     - |  491 | `		/* Fetch the info entry for the field */` |
|   925 |  492 | `		pInfo = 0;` |
|   925 |  493 | `		xtype = PH7_FMT_ERROR;` |
|   925 |  494 | `		c = zIn[0];` |
|   925 |  495 | `		zIn++; /* Jump the format specifer */` |
|  3485 |  496 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|  3485 |  497 | `			if( c==aFmt[idx].fmttype ){` |
|   925 |  498 | `				pInfo = &aFmt[idx];` |
|   925 |  499 | `				xtype = pInfo->type;` |
|   925 |  500 | `				break;` |
|     - |  501 | `			}` |
|  1283 |  502 | `		}` |
|   925 |  503 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   925 |  504 | `		length = 0;` |
|     - |  505 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|     - |  506 | `		 /*` |
|     - |  507 | `		  ** At this point, variables are initialized as follows:` |
|     - |  508 | `		  **` |
|     - |  509 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|     - |  510 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|     - |  511 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|     - |  512 | `		  **                               field width was negative.` |
|     - |  513 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|     - |  514 | `		  **                               the conversion character.` |
|     - |  515 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|     - |  516 | `		  **   width                       The specified field width.  This is` |
|     - |  517 | `		  **                               always non-negative.  Zero is the default.` |
|     - |  518 | `		  **   precision                   The specified precision.  The default` |
|     - |  519 | `		  **                               is -1.` |
|     - |  520 | `		  */` |
|   925 |  521 | `		switch(xtype){` |
|     5 |  522 | `		case PH7_FMT_PERCENT:` |
|     - |  523 | `			/* A literal percent character */` |
|    11 |  524 | `			zWorker[0] = '%';` |
|    11 |  525 | `			length = (int)sizeof(char);` |
|    11 |  526 | `			break;` |
|     2 |  527 | `		case PH7_FMT_CHARX:` |
|     - |  528 | `			/* The argument is treated as an integer, and presented as the character` |
|     - |  529 | `			 * with that ASCII value` |
|     - |  530 | `			 */` |
|     5 |  531 | `			pArg = NEXT_ARG;` |
|     5 |  532 | `			if( pArg == 0 ){` |
|   ! 0 |  533 | `				c = 0;` |
|   ! 0 |  534 | `			}else{` |
|     5 |  535 | `				c = ph7_value_to_int(pArg);` |
|     - |  536 | `			}` |
|     - |  537 | `			/* NUL byte is an acceptable value */` |
|     5 |  538 | `			zWorker[0] = (char)c;` |
|     5 |  539 | `			length = (int)sizeof(char);` |
|     5 |  540 | `			break;` |
|   190 |  541 | `		case PH7_FMT_STRING:` |
|     - |  542 | `			/* the argument is treated as and presented as a string */` |
|   381 |  543 | `			pArg = NEXT_ARG;` |
|   381 |  544 | `			if( pArg == 0 ){` |
|   ! 0 |  545 | `				length = 0;` |
|   ! 0 |  546 | `			}else{` |
|   381 |  547 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|     - |  548 | `			}` |
|   381 |  549 | `			if( length < 1 ){` |
|     - |  550 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|     - |  551 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|     - |  552 | `				 * absent optional part gained a stray space. */` |
|     9 |  553 | `				zBuf = "";` |
|     9 |  554 | `				length = 0;` |
|     4 |  555 | `			}` |
|   381 |  556 | `			if( precision>=0 && precision<length ){` |
|     3 |  557 | `				length = precision;` |
|     1 |  558 | `			}` |
|   381 |  559 | `			if( flag_zeropad ){` |
|     - |  560 | `				/* zero-padding works on strings too */` |
|   103 |  561 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   101 |  562 | `					spaces[idx] = '0';` |
|    51 |  563 | `				}` |
|     1 |  564 | `			}` |
|   381 |  565 | `			break;` |
|   164 |  566 | `		case PH7_FMT_RADIX:` |
|   329 |  567 | `			pArg = NEXT_ARG;` |
|   329 |  568 | `			if( pArg == 0 ){` |
|   ! 0 |  569 | `				iVal = 0;` |
|   ! 0 |  570 | `			}else{` |
|   329 |  571 | `				iVal = ph7_value_to_int64(pArg);` |
|     - |  572 | `			}` |
|     - |  573 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   329 |  574 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|   ! 0 |  575 | `				precision = PH7_FMT_BUFSIZ-40;` |
|   ! 0 |  576 | `			}` |
|     - |  577 | `#if 1` |
|     - |  578 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|     - |  579 | `        ** I think this is stupid.*/` |
|   329 |  580 | `        if( iVal==0 ) flag_alternateform = 0;` |
|     - |  581 | `#else` |
|     - |  582 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|     - |  583 | `        ** but leave the prefix for hex.*/` |
|     - |  584 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|     - |  585 | `#endif` |
|   329 |  586 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|   305 |  587 | `          if( iVal<0 ){` |
|    25 |  588 | `            iVal = -iVal;` |
|     - |  589 | `			/* Ticket 1433-003 */` |
|    25 |  590 | `			if( iVal < 0 ){` |
|     - |  591 | `				/* Overflow */` |
|   ! 0 |  592 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  593 | `			}` |
|    25 |  594 | `            prefix = '-';` |
|   293 |  595 | `          }else if( flag_plussign )  prefix = '+';` |
|   279 |  596 | `          else if( flag_blanksign )  prefix = ' ';` |
|   277 |  597 | `          else                       prefix = 0;` |
|   153 |  598 | `        }else{` |
|    25 |  599 | `			if( iVal<0 ){` |
|   ! 0 |  600 | `				iVal = -iVal;` |
|     - |  601 | `				/* Ticket 1433-003 */` |
|   ! 0 |  602 | `				if( iVal < 0 ){` |
|     - |  603 | `					/* Overflow */` |
|   ! 0 |  604 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  605 | `				}` |
|   ! 0 |  606 | `			}` |
|    25 |  607 | `			prefix = 0;` |
|     - |  608 | `		}` |
|   329 |  609 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|   185 |  610 | `          precision = width-(prefix!=0);` |
|    92 |  611 | `        }` |
|   329 |  612 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|     - |  613 | `        {` |
|     - |  614 | `          register char *cset;      /* Use registers for speed */` |
|     - |  615 | `          register int base;` |
|   329 |  616 | `          cset = pInfo->charset;` |
|   329 |  617 | `          base = pInfo->base;` |
|   164 |  618 | `          do{                                           /* Convert to ascii */` |
|   409 |  619 | `            *(--zBuf) = cset[iVal%base];` |
|   409 |  620 | `            iVal = iVal/base;` |
|   409 |  621 | `          }while( iVal>0 );` |
|     - |  622 | `        }` |
|   329 |  623 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   531 |  624 | `        for(idx=precision-length; idx>0; idx--){` |
|   203 |  625 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|   102 |  626 | `        }` |
|   329 |  627 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|   329 |  628 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|     - |  629 | `          char *pre, x;` |
|   ! 0 |  630 | `          pre = pInfo->prefix;` |
|   ! 0 |  631 | `          if( *zBuf!=pre[0] ){` |
|   ! 0 |  632 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|   ! 0 |  633 | `          }` |
|   ! 0 |  634 | `        }` |
|   329 |  635 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   329 |  636 | `		break;` |
|   100 |  637 | `		case PH7_FMT_FLOAT:` |
|     - |  638 | `		case PH7_FMT_EXP:` |
|     - |  639 | `		case PH7_FMT_GENERIC:{` |
|     - |  640 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     - |  641 | `		double realvalue;` |
|     - |  642 | `		char zFmt[8];` |
|     - |  643 | `		int nOut, nFmt;` |
|   203 |  644 | `		pArg = NEXT_ARG;` |
|   203 |  645 | `		if( pArg == 0 ){` |
|   ! 0 |  646 | `			realvalue = 0;` |
|   ! 0 |  647 | `		}else{` |
|   203 |  648 | `			realvalue = ph7_value_to_double(pArg);` |
|     - |  649 | `		}` |
|     - |  650 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|     - |  651 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|   203 |  652 | `		if( PH7_IS_NAN(realvalue) ){` |
|    21 |  653 | `			zBuf = "NaN";` |
|    21 |  654 | `			length = 3;` |
|    21 |  655 | `			width = 0;` |
|    21 |  656 | `			break;` |
|     - |  657 | `		}` |
|   183 |  658 | `		if( PH7_IS_INF(realvalue) ){` |
|    37 |  659 | `			if( realvalue < 0.0 ){` |
|    15 |  660 | `				zBuf = "-INF";` |
|    15 |  661 | `				length = 4;` |
|     8 |  662 | `			}else{` |
|    23 |  663 | `				zBuf = "INF";` |
|    23 |  664 | `				length = 3;` |
|     - |  665 | `			}` |
|    37 |  666 | `			width = 0;` |
|    37 |  667 | `			break;` |
|     - |  668 | `		}` |
|   147 |  669 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|   147 |  670 | `		if( precision > 53 ){` |
|     - |  671 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|     - |  672 | `			 * (message prefixed with the active function's name, like` |
|     - |  673 | `			 * php_error_docref). */` |
|     - |  674 | `			char zMsg[160];` |
|     4 |  675 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  676 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|     2 |  677 | `				&pCtx->pFunc->sName,precision,53);` |
|     3 |  678 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|     3 |  679 | `			precision = 53;` |
|     1 |  680 | `		}` |
|     - |  681 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|     - |  682 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|   147 |  683 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|     9 |  684 | `			realvalue = 0.0;` |
|     4 |  685 | `		}` |
|     - |  686 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|     - |  687 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|     - |  688 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|     - |  689 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|     - |  690 | `		 * expansion), then post-process into php's exact shapes below. */` |
|   147 |  691 | `		nFmt = 0;` |
|   147 |  692 | `		zFmt[nFmt++] = '%';` |
|   147 |  693 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|     - |  694 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|     - |  695 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|   147 |  696 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|   147 |  697 | `		zFmt[nFmt++] = '.';` |
|   147 |  698 | `		zFmt[nFmt++] = '*';` |
|   195 |  699 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|    32 |  700 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|    32 |  701 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|   147 |  702 | `		zFmt[nFmt] = 0;` |
|   147 |  703 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|   147 |  704 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|     - |  705 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|     - |  706 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|   ! 0 |  707 | `			nOut = (int)SyStrlen(zWorker);` |
|   ! 0 |  708 | `		}` |
|   147 |  709 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|   147 |  710 | `		zBuf = zWorker;` |
|   147 |  711 | `		length = nOut;` |
|     - |  712 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|     - |  713 | `		 * by snprintf) and the first digit, as before. */` |
|   147 |  714 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|     - |  715 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|     - |  716 | `        ** set and we are not left justified */` |
|   147 |  717 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|     - |  718 | `          int i;` |
|     9 |  719 | `          int nPad = width - length;` |
|    63 |  720 | `          for(i=width; i>=nPad; i--){` |
|    55 |  721 | `            zBuf[i] = zBuf[i-nPad];` |
|    28 |  722 | `          }` |
|     9 |  723 | `          i = prefix!=0;` |
|    39 |  724 | `          while( nPad-- ) zBuf[i++] = '0';` |
|     9 |  725 | `          length = width;` |
|     4 |  726 | `        }` |
|     - |  727 | `#else` |
|     - |  728 | `         zBuf = " ";` |
|     - |  729 | `		 length = (int)sizeof(char);` |
|     - |  730 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|   147 |  731 | `		 break;` |
|     - |  732 | `							 }` |
|   ! 0 |  733 | `		default:` |
|     - |  734 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|     - |  735 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|     - |  736 | `			 * no-op that emits nothing. */` |
|   ! 0 |  737 | `			length = 0;` |
|   ! 0 |  738 | `			break;` |
|     - |  739 | `		}` |
|     - |  740 | `		 /*` |
|     - |  741 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|     - |  742 | `		 ** "length" characters long.The field width is "width".Do` |
|     - |  743 | `		 ** the output.` |
|     - |  744 | `		 */` |
|   925 |  745 | `    if( !flag_leftjustify ){` |
|     - |  746 | `      register int nspace;` |
|   907 |  747 | `      nspace = width-length;` |
|   907 |  748 | `      if( nspace>0 ){` |
|    37 |  749 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  750 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  751 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  752 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  753 | `			}` |
|   ! 0 |  754 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  755 | `        }` |
|    37 |  756 | `        if( nspace>0 ){` |
|    37 |  757 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    37 |  758 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  759 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  760 | `			}` |
|    18 |  761 | `		}` |
|    18 |  762 | `      }` |
|   452 |  763 | `    }` |
|   925 |  764 | `    if( length>0 ){` |
|   917 |  765 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   917 |  766 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  767 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  768 | `		}` |
|   457 |  769 | `    }` |
|   925 |  770 | `    if( flag_leftjustify ){` |
|     - |  771 | `      register int nspace;` |
|    19 |  772 | `      nspace = width-length;` |
|    19 |  773 | `      if( nspace>0 ){` |
|    15 |  774 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  775 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  776 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  777 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  778 | `			}` |
|   ! 0 |  779 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  780 | `        }` |
|    15 |  781 | `        if( nspace>0 ){` |
|    15 |  782 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    15 |  783 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  784 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  785 | `			}` |
|     7 |  786 | `		}` |
|     7 |  787 | `      }` |
|     9 |  788 | `    }` |
|     3 |  789 | ` }/* for(;;) */` |
|   467 |  790 | `	return SXRET_OK;` |
|   235 |  791 | `}` |
|     - |  792 | `/*` |
|     - |  793 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|     - |  794 | ` */` |
|   538 |  795 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     3 |  796 | `{` |
|     - |  797 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|     - |  798 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|     - |  799 | `	 * non-OK rc also stops the format loop. */` |
|   541 |  800 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|   541 |  801 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|   541 |  802 | `	return *pRc;` |
|     3 |  803 | `}` |
|     - |  804 | `/*` |
|     - |  805 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|     - |  806 | ` *  Return a formatted string.` |
|     - |  807 | ` * Parameters` |
|     - |  808 | ` *  $format` |
|     - |  809 | ` *    The format string (see block comment above)` |
|     - |  810 | ` * Return` |
|     - |  811 | ` *  A string produced according to the formatting string format.` |
|     - |  812 | ` */` |
|   274 |  813 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  814 | `{` |
|     - |  815 | `	const char *zFormat;` |
|   277 |  816 | `	sxi32 rc = SXRET_OK;` |
|     - |  817 | `	int nLen;` |
|   277 |  818 | `	if( nArg < 1 ){` |
|     - |  819 | `		/* Missing arguments,return the empty string */` |
|   ! 0 |  820 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  821 | `		return PH7_OK;` |
|     - |  822 | `	}` |
|     - |  823 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|   277 |  824 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   277 |  825 | `	if( rc != PH7_OK ){` |
|   ! 0 |  826 | `		return rc;` |
|     - |  827 | `	}` |
|     - |  828 | `	/* Extract the string format (scalars/null coerce). */` |
|   277 |  829 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   277 |  830 | `	if( nLen < 1 ){` |
|     - |  831 | `		/* Empty string */` |
|   ! 0 |  832 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  833 | `		return PH7_OK;` |
|     - |  834 | `	}` |
|     - |  835 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - |  836 | `	 * output; propagate the throw status verbatim. */` |
|   277 |  837 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   277 |  838 | `	if( rc != PH7_OK ){` |
|    17 |  839 | `		return rc;` |
|     - |  840 | `	}` |
|     - |  841 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|   261 |  842 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   261 |  843 | `	if( rc != PH7_OK ){` |
|    29 |  844 | `		return rc;` |
|     - |  845 | `	}` |
|     - |  846 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|   233 |  847 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|   233 |  848 | `	if( rc != SXRET_OK ){` |
|     - |  849 | `		/* The result append ran out of memory: raise a fatal rather than` |
|     - |  850 | `		 * returning a silently-truncated string. */` |
|   ! 0 |  851 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  852 | `	}` |
|   233 |  853 | `	return PH7_OK;` |
|   140 |  854 | `}` |
|     - |  855 | `/*` |
|     - |  856 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|     - |  857 | ` */` |
|  1210 |  858 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     1 |  859 | `{` |
|  1211 |  860 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|     - |  861 | `	/* Call the VM output consumer directly */` |
|  1211 |  862 | `	ph7_context_output(pCtx,zInput,nLen);` |
|     - |  863 | `	/* Increment counter */` |
|  1211 |  864 | `	*pCounter += nLen;` |
|  1211 |  865 | `	return PH7_OK;` |
|     1 |  866 | `}` |
|     - |  867 | `/*` |
|     - |  868 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|     - |  869 | ` *  Output a formatted string.` |
|     - |  870 | ` * Parameters` |
|     - |  871 | ` *  $format` |
|     - |  872 | ` *   See sprintf() for a description of format.` |
|     - |  873 | ` * Return` |
|     - |  874 | ` *  The length of the outputted string.` |
|     - |  875 | ` */` |
|   210 |  876 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  877 | `{` |
|   211 |  878 | `	ph7_int64 nCounter = 0;` |
|     - |  879 | `	const char *zFormat;` |
|     - |  880 | `	int nLen;` |
|   211 |  881 | `	if( nArg < 1 ){` |
|     - |  882 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  883 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  884 | `		return PH7_OK;` |
|     - |  885 | `	}` |
|     - |  886 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|     - |  887 | `	{` |
|   211 |  888 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   211 |  889 | `		if( rcf != PH7_OK ){` |
|   ! 0 |  890 | `			return rcf;` |
|     - |  891 | `		}` |
|     - |  892 | `	}` |
|     - |  893 | `	/* Extract the string format (scalars/null coerce). */` |
|   211 |  894 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   211 |  895 | `	if( nLen < 1 ){` |
|     - |  896 | `		/* Empty string */` |
|   ! 0 |  897 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  898 | `		return PH7_OK;` |
|     - |  899 | `	}` |
|     - |  900 | `	{` |
|     - |  901 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|     - |  902 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|   211 |  903 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   211 |  904 | `		if( rcv != PH7_OK ){` |
|     3 |  905 | `			return rcv;` |
|     - |  906 | `		}` |
|     - |  907 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|     - |  908 | `		 * before any output; propagate the throw status verbatim. */` |
|   209 |  909 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   209 |  910 | `		if( rcv != PH7_OK ){` |
|   ! 0 |  911 | `			return rcv;` |
|     - |  912 | `		}` |
|     - |  913 | `	}` |
|     - |  914 | `	/* Format the string */` |
|   209 |  915 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|     - |  916 | `	/* Return the length of the outputted string */` |
|   209 |  917 | `	ph7_result_int64(pCtx,nCounter);` |
|   209 |  918 | `	return PH7_OK;` |
|   106 |  919 | `}` |
|     - |  920 | `/*` |
|     - |  921 | ` * int vprintf(string $format,array $args)` |
|     - |  922 | ` *  Output a formatted string.` |
|     - |  923 | ` * Parameters` |
|     - |  924 | ` *  $format` |
|     - |  925 | ` *   See sprintf() for a description of format.` |
|     - |  926 | ` * Return` |
|     - |  927 | ` *  The length of the outputted string.` |
|     - |  928 | ` */` |
|     4 |  929 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  930 | `{` |
|     5 |  931 | `	ph7_int64 nCounter = 0;` |
|     - |  932 | `	const char *zFormat;` |
|     - |  933 | `	ph7_hashmap *pMap;` |
|     - |  934 | `	SySet sArg;` |
|     - |  935 | `	int nLen,n;` |
|     - |  936 | `	sxi32 rcFmt;` |
|     5 |  937 | `	if( nArg < 2 ){` |
|     - |  938 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  939 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  940 | `		return PH7_OK;` |
|     - |  941 | `	}` |
|     - |  942 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     5 |  943 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     5 |  944 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  945 | `		return rcFmt;` |
|     - |  946 | `	}` |
|     5 |  947 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - |  948 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - |  949 | `		char zBuf[64];` |
|     4 |  950 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  951 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     2 |  952 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - |  953 | `	}` |
|     - |  954 | `	/* Extract the string format (scalars/null coerce). */` |
|     3 |  955 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     3 |  956 | `	if( nLen < 1 ){` |
|     - |  957 | `		/* Empty string */` |
|   ! 0 |  958 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  959 | `		return PH7_OK;` |
|     - |  960 | `	}` |
|     - |  961 | `	/* Point to the hashmap */` |
|     3 |  962 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - |  963 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|     - |  964 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|     - |  965 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|     - |  966 | `	 * rather than the missing specifier. */` |
|     3 |  967 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     3 |  968 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  969 | `		return rcFmt;` |
|     - |  970 | `	}` |
|     - |  971 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|     - |  972 | `	 * any output; propagate the throw status verbatim. */` |
|     3 |  973 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     3 |  974 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  975 | `		return rcFmt;` |
|     - |  976 | `	}` |
|     - |  977 | `	/* Extract arguments from the hashmap */` |
|     3 |  978 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - |  979 | `	/* Format the string */` |
|     3 |  980 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|     - |  981 | `	/* Release the container */` |
|     3 |  982 | `	SySetRelease(&sArg);` |
|     - |  983 | `	/* Return the length of the outputted string */` |
|     3 |  984 | `	ph7_result_int64(pCtx,nCounter);` |
|     3 |  985 | `	return PH7_OK;` |
|     3 |  986 | `}` |
|     - |  987 | `/*` |
|     - |  988 | ` * int vsprintf(string $format,array $args)` |
|     - |  989 | ` *  Output a formatted string.` |
|     - |  990 | ` * Parameters` |
|     - |  991 | ` *  $format` |
|     - |  992 | ` *   See sprintf() for a description of format.` |
|     - |  993 | ` * Return` |
|     - |  994 | ` *  A string produced according to the formatting string format.` |
|     - |  995 | ` */` |
|    26 |  996 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  997 | `{` |
|     - |  998 | `	const char *zFormat;` |
|     - |  999 | `	ph7_hashmap *pMap;` |
|     - | 1000 | `	SySet sArg;` |
|    27 | 1001 | `	sxi32 rc = SXRET_OK;` |
|     - | 1002 | `	sxi32 rcFmt;` |
|     - | 1003 | `	int nLen,n;` |
|    27 | 1004 | `	if( nArg < 2 ){` |
|     - | 1005 | `		/* Missing arguments,return the empty string */` |
|   ! 0 | 1006 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1007 | `		return PH7_OK;` |
|     - | 1008 | `	}` |
|     - | 1009 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|    27 | 1010 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    27 | 1011 | `	if( rc != PH7_OK ){` |
|   ! 0 | 1012 | `		return rc;` |
|     - | 1013 | `	}` |
|    27 | 1014 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - | 1015 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - | 1016 | `		char zBuf[64];` |
|    13 | 1017 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1018 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     8 | 1019 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - | 1020 | `	}` |
|     - | 1021 | `	/* Extract the string format (scalars/null coerce). */` |
|    19 | 1022 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1023 | `	if( nLen < 1 ){` |
|     - | 1024 | `		/* Empty string */` |
|   ! 0 | 1025 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1026 | `		return PH7_OK;` |
|     - | 1027 | `	}` |
|     - | 1028 | `	/* Point to hashmap */` |
|    19 | 1029 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - | 1030 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|     - | 1031 | `	 * php runs this BEFORE validating the specifiers. */` |
|    19 | 1032 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|    19 | 1033 | `	if( rcFmt != PH7_OK ){` |
|     7 | 1034 | `		return rcFmt;` |
|     - | 1035 | `	}` |
|     - | 1036 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|     - | 1037 | `	 * any output; propagate the throw status verbatim. */` |
|    13 | 1038 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    13 | 1039 | `	if( rcFmt != PH7_OK ){` |
|     3 | 1040 | `		return rcFmt;` |
|     - | 1041 | `	}` |
|     - | 1042 | `	/* Extract arguments from the hashmap */` |
|    11 | 1043 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - | 1044 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    11 | 1045 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|     - | 1046 | `	/* Release the container */` |
|    11 | 1047 | `	SySetRelease(&sArg);` |
|    11 | 1048 | `	if( rc != SXRET_OK ){` |
|     - | 1049 | `		/* The result append ran out of memory: raise a fatal. */` |
|   ! 0 | 1050 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1051 | `	}` |
|    11 | 1052 | `	return PH7_OK;` |
|    14 | 1053 | `}` |
|     - | 1054 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1055 |  |
