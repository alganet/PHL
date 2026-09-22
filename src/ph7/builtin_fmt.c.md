# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 507/589 lines (86.08%)

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
|   1028 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|      5 |   95 | `{` |
|   1033 |   96 | `	const char *zEnd = &zIn[nByte];` |
|      - |   97 | `	int c,idx;` |
|  10757 |   98 | `	while( zIn < zEnd ){` |
|   9763 |   99 | `		if( zIn[0] != '%' ){` |
|   7147 |  100 | `			zIn++;` |
|   7147 |  101 | `			continue;` |
|      - |  102 | `		}` |
|   2621 |  103 | `		zIn++; /* jump the percent sign */` |
|      - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - |  106 | `		 * unknown specifier, matching php. */` |
|   3675 |  107 | `		while( zIn < zEnd ){` |
|   3665 |  108 | `			c = zIn[0];` |
|   3665 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   1047 |  110 | `				zIn++;` |
|   1047 |  111 | `				continue;` |
|      - |  112 | `			}` |
|   2623 |  113 | `			if( c=='\'' ){` |
|     13 |  114 | `				zIn++;` |
|     13 |  115 | `				if( zIn < zEnd ){` |
|     13 |  116 | `					zIn++; /* the custom pad character */` |
|      6 |  117 | `				}` |
|     13 |  118 | `				continue;` |
|      - |  119 | `			}` |
|   2611 |  120 | `			break;` |
|    ! 0 |  121 | `		}` |
|      - |  122 | `		/* field width */` |
|   4341 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   1725 |  124 | `			zIn++;` |
|      5 |  125 | `		}` |
|      - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   2621 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     14 |  129 | `			zIn++;` |
|     16 |  130 | `			while( zIn < zEnd ){` |
|     14 |  131 | `				c = zIn[0];` |
|     14 |  132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 |  133 | `					zIn++;` |
|    ! 0 |  134 | `					continue;` |
|      - |  135 | `				}` |
|     14 |  136 | `				if( c=='\'' ){` |
|      3 |  137 | `					zIn++;` |
|      3 |  138 | `					if( zIn < zEnd ){` |
|      3 |  139 | `						zIn++;` |
|      1 |  140 | `					}` |
|      3 |  141 | `					continue;` |
|      - |  142 | `				}` |
|     12 |  143 | `				break;` |
|    ! 0 |  144 | `			}` |
|     22 |  145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|      9 |  146 | `				zIn++;` |
|      1 |  147 | `			}` |
|      6 |  148 | `		}` |
|      - |  149 | `		/* precision */` |
|   2621 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    175 |  151 | `			zIn++;` |
|    365 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    195 |  153 | `				zIn++;` |
|      5 |  154 | `			}` |
|     85 |  155 | `		}` |
|      - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   2621 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 |  158 | `			zIn++;` |
|      5 |  159 | `		}` |
|   2621 |  160 | `		if( zIn >= zEnd ){` |
|      - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|      - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|     15 |  163 | `			*pbDangling = TRUE;` |
|     15 |  164 | `			return FALSE;` |
|      - |  165 | `		}` |
|   2607 |  166 | `		c = zIn[0];` |
|   2607 |  167 | `		zIn++; /* jump the conversion specifier */` |
|   7287 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   7267 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   2587 |  170 | `				break;` |
|      - |  171 | `			}` |
|   2345 |  172 | `		}` |
|   2607 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     21 |  174 | `			*pBad = c; /* unknown specifier */` |
|     21 |  175 | `			return TRUE;` |
|      - |  176 | `		}` |
|      5 |  177 | `	}` |
|    999 |  178 | `	return FALSE;` |
|    519 |  179 | `}` |
|      - |  180 | `/*` |
|      - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - |  186 | ` * Returns PH7_OK when the format is valid.` |
|      - |  187 | ` */` |
|   1028 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      5 |  189 | `{` |
|   1033 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   1033 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|     31 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     10 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - |  194 | `	}` |
|   1013 |  195 | `	if( bDangling ){` |
|     15 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  197 | `			"Missing format specifier at end of string");` |
|      - |  198 | `	}` |
|    999 |  199 | `	return PH7_OK;` |
|    519 |  200 | `}` |
|      - |  201 | `/*` |
|      - |  202 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|      - |  203 | ` * sequential (non-positional) conversion count and the highest positional index` |
|      - |  204 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|      - |  205 | ` */` |
|   1056 |  206 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|      5 |  207 | `{` |
|   1061 |  208 | `	const char *zEnd = &zIn[nByte];` |
|   1061 |  209 | `	int c,seq = 0,maxpos = 0;` |
|  10881 |  210 | `	while( zIn < zEnd ){` |
|   9845 |  211 | `		int numVal = 0,pos = 0;` |
|   9845 |  212 | `		if( zIn[0] != '%' ){` |
|   7185 |  213 | `			zIn++;` |
|   7185 |  214 | `			continue;` |
|      - |  215 | `		}` |
|   2665 |  216 | `		zIn++; /* jump the percent sign */` |
|      - |  217 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|   3719 |  218 | `		while( zIn < zEnd ){` |
|   3703 |  219 | `			c = zIn[0];` |
|   3703 |  220 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|   2661 |  221 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|   2649 |  222 | `			break;` |
|    ! 0 |  223 | `		}` |
|      - |  224 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|   4391 |  225 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   1731 |  226 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|   1731 |  227 | `			zIn++;` |
|      5 |  228 | `		}` |
|   2665 |  229 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|     20 |  230 | `			pos = numVal;` |
|     20 |  231 | `			zIn++;` |
|      - |  232 | `			/* flags then width may follow the positional marker */` |
|     22 |  233 | `			while( zIn < zEnd ){` |
|     20 |  234 | `				c = zIn[0];` |
|     20 |  235 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|     20 |  236 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|     18 |  237 | `				break;` |
|    ! 0 |  238 | `			}` |
|     28 |  239 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|      9 |  240 | `		}` |
|      - |  241 | `		/* precision */` |
|   2665 |  242 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|    175 |  243 | `			zIn++;` |
|    365 |  244 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     85 |  245 | `		}` |
|      - |  246 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   2665 |  247 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|   2665 |  248 | `		if( zIn >= zEnd ){` |
|      - |  249 | `			/* A dangling '%' still COUNTS as needing a value: php reports` |
|      - |  250 | `			 * sprintf("%") as "2 arguments are required, 1 given" and only` |
|      - |  251 | `			 * raises the missing-specifier ValueError once the count is met. */` |
|     21 |  252 | `			if( pos > 0 ){` |
|      3 |  253 | `				if( pos > maxpos ){ maxpos = pos; }` |
|      2 |  254 | `			}else{` |
|     19 |  255 | `				seq++;` |
|      - |  256 | `			}` |
|     21 |  257 | `			break;` |
|      - |  258 | `		}` |
|   2645 |  259 | `		c = zIn[0];` |
|   2645 |  260 | `		zIn++; /* jump the conversion specifier */` |
|   2645 |  261 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   2635 |  262 | `		if( pos > 0 ){` |
|     18 |  263 | `			if( pos > maxpos ){ maxpos = pos; }` |
|     10 |  264 | `		}else{` |
|   2619 |  265 | `			seq++;` |
|      - |  266 | `		}` |
|      5 |  267 | `	}` |
|   1061 |  268 | `	return seq > maxpos ? seq : maxpos;` |
|      5 |  269 | `}` |
|      - |  270 | `/*` |
|      - |  271 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|      - |  272 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|      - |  273 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|      - |  274 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|      - |  275 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|      - |  276 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|      - |  277 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|      - |  278 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|      - |  279 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|      - |  280 | ` * when enough.` |
|      - |  281 | ` */` |
|   1056 |  282 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|      5 |  283 | `{` |
|   1061 |  284 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|   1061 |  285 | `	if( nValues < required ){` |
|     29 |  286 | `		if( bVararg ){` |
|     13 |  287 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 |  288 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|      - |  289 | `		}` |
|     31 |  290 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     10 |  291 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|      - |  292 | `	}` |
|   1033 |  293 | `	return PH7_OK;` |
|    533 |  294 | `}` |
|      - |  295 | `/*` |
|      - |  296 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - |  297 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - |  298 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - |  299 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - |  300 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - |  301 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - |  302 | ` */` |
|      - |  303 | `/*` |
|      - |  304 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|      - |  305 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|      - |  306 | ` */` |
|     28 |  307 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|      2 |  308 | `{` |
|     30 |  309 | `	if( !ph7_value_is_resource(pArg) ){` |
|      - |  310 | `		char zBuf[64];` |
|      4 |  311 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  312 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      1 |  313 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - |  314 | `	}` |
|     28 |  315 | `	return PH7_OK;` |
|     16 |  316 | `}` |
|   1066 |  317 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      5 |  318 | `{` |
|   1071 |  319 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - |  320 | `		char zBuf[64];` |
|    ! 0 |  321 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  322 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|    ! 0 |  323 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - |  324 | `	}` |
|   1071 |  325 | `	return PH7_OK;` |
|    538 |  326 | `}` |
|      - |  327 | `/*` |
|      - |  328 | ` * Format a given string.` |
|      - |  329 | ` * The root program.  All variations call this core.` |
|      - |  330 | ` * INPUTS:` |
|      - |  331 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - |  332 | ` *            1. A pointer to the call context.` |
|      - |  333 | ` *            2. A pointer to the list of characters to be output` |
|      - |  334 | ` *               (Note, this list is NOT null terminated.)` |
|      - |  335 | ` *            3. An integer number of characters to be output.` |
|      - |  336 | ` *               (Note: This number might be zero.)` |
|      - |  337 | ` *            4. Upper layer private data.` |
|      - |  338 | ` *   zIn       This is the format string, as in the usual print.` |
|      - |  339 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - |  340 | ` */` |
|    994 |  341 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - |  342 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - |  343 | `	ph7_context *pCtx,  /* call context */` |
|      - |  344 | `	const char *zIn,    /* Format string */` |
|      - |  345 | `	int nByte,          /* Format string length */` |
|      - |  346 | `	int nArg,           /* Total argument of the given arguments */` |
|      - |  347 | `	ph7_value **apArg,  /* User arguments */` |
|      - |  348 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - |  349 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - |  350 | `	)` |
|      5 |  351 | `{` |
|    999 |  352 | `	char spaces[] = "                                                  ";` |
|      - |  353 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    999 |  354 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - |  355 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - |  356 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - |  357 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - |  358 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - |  359 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - |  360 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - |  361 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - |  362 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - |  363 | `	ph7_int64 iVal;` |
|      - |  364 | `	int precision;           /* Precision of the current field */` |
|      - |  365 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - |  366 | `	int c,rc,n;` |
|    999 |  367 | `	ph7_value *pThrowArg = 0; /* First not-stringable %s argument; throws at the end */` |
|      - |  368 | `	int length;              /* Length of the field */` |
|      - |  369 | `	int prefix;` |
|      - |  370 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - |  371 | `	int width;               /* Width of the current field */` |
|      - |  372 | `	int idx;` |
|    999 |  373 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - |  374 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - |  375 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - |  376 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - |  377 | `	 * seen here is always valid. */` |
|      - |  378 | `	/* Start the format process */` |
|   1786 |  379 | `	for(;;){` |
|   3577 |  380 | `		zCur = zIn;` |
|  10693 |  381 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   7121 |  382 | `			zIn++;` |
|      5 |  383 | `		}` |
|   3577 |  384 | `		if( zCur < zIn ){` |
|      - |  385 | `			/* Consume chunk verbatim */` |
|   2427 |  386 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   2427 |  387 | `			if( rc != SXRET_OK ){` |
|      - |  388 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 |  389 | `				break;` |
|      - |  390 | `			}` |
|   1211 |  391 | `		}` |
|   3577 |  392 | `		if( zIn >= zEnd ){` |
|      - |  393 | `			/* No more input to process,break immediately */` |
|    997 |  394 | `			break;` |
|      - |  395 | `		}` |
|      - |  396 | `		/* Find out what flags are present */` |
|   2585 |  397 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|   2580 |  398 | `			flag_alternateform = flag_zeropad = 0;` |
|      - |  399 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|      - |  400 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|      - |  401 | `		 * php resets the pad character for every specifier. */` |
| 131585 |  402 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|   2585 |  403 | `		zIn++; /* Jump the precent sign */` |
|   1290 |  404 | `		do{` |
|   3637 |  405 | `			c = zIn[0];` |
|   3637 |  406 | `			switch( c ){` |
|    783 |  407 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 |  408 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 |  409 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    252 |  410 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      6 |  411 | `			case '\'':` |
|     13 |  412 | `				zIn++;` |
|     13 |  413 | `				if( zIn < zEnd ){` |
|      - |  414 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|     13 |  415 | `					c = zIn[0];` |
|    613 |  416 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    601 |  417 | `						spaces[idx] = (char)c;` |
|    301 |  418 | `					}` |
|     13 |  419 | `					c = 0;` |
|      6 |  420 | `				}` |
|     12 |  421 | `				break;` |
|   2580 |  422 | `			default:                                       break;` |
|      - |  423 | `			}` |
|   3637 |  424 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - |  425 | `		/* Get the field width */` |
|   2585 |  426 | `		width = 0;` |
|   5591 |  427 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   1721 |  428 | `			width = width*10 + (zIn[0] - '0');` |
|   1721 |  429 | `			zIn++;` |
|      5 |  430 | `		}` |
|   2585 |  431 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - |  432 | `			/* Position specifer */` |
|     12 |  433 | `			if( width > 0 ){` |
|     12 |  434 | `				n = width;` |
|     12 |  435 | `				if( vf && n > 0 ){` |
|    ! 0 |  436 | `					n--;` |
|    ! 0 |  437 | `				}` |
|      5 |  438 | `			}` |
|     12 |  439 | `			zIn++;` |
|     12 |  440 | `			width = 0;` |
|      - |  441 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - |  442 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - |  443 | `			 * not just zero-padding. */` |
|      5 |  444 | `			do{` |
|     14 |  445 | `				c = zIn[0];` |
|     14 |  446 | `				switch( c ){` |
|    ! 0 |  447 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 |  448 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 |  449 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 |  450 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|      1 |  451 | `				case '\'':` |
|      3 |  452 | `					zIn++;` |
|      3 |  453 | `					if( zIn < zEnd ){` |
|      3 |  454 | `						c = zIn[0];` |
|    103 |  455 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 |  456 | `							spaces[idx] = (char)c;` |
|     51 |  457 | `						}` |
|      3 |  458 | `						c = 0;` |
|      1 |  459 | `					}` |
|      2 |  460 | `					break;` |
|     10 |  461 | `				default:                                       break;` |
|      - |  462 | `				}` |
|     14 |  463 | `			}while( c==0 && (zIn++ < zEnd) );` |
|     25 |  464 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|      9 |  465 | `				width = width*10 + (zIn[0] - '0');` |
|      9 |  466 | `				zIn++;` |
|      1 |  467 | `			}` |
|      5 |  468 | `		}` |
|   2585 |  469 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 |  470 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 |  471 | `		}` |
|      - |  472 | `		/* Get the precision */` |
|   2585 |  473 | `		precision = -1;` |
|   2585 |  474 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|    175 |  475 | `			precision = 0;` |
|    175 |  476 | `			zIn++;` |
|    450 |  477 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    195 |  478 | `				precision = precision*10 + (zIn[0] - '0');` |
|    195 |  479 | `				zIn++;` |
|      5 |  480 | `			}` |
|     85 |  481 | `		}` |
|      - |  482 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - |  483 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - |  484 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   2585 |  485 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 |  486 | `			zIn++;` |
|      4 |  487 | `		}` |
|   2585 |  488 | `		if( zIn >= zEnd ){` |
|      - |  489 | `			/* No more input */` |
|    ! 0 |  490 | `			break;` |
|      - |  491 | `		}` |
|      - |  492 | `		/* Fetch the info entry for the field */` |
|   2585 |  493 | `		pInfo = 0;` |
|   2585 |  494 | `		xtype = PH7_FMT_ERROR;` |
|   2585 |  495 | `		c = zIn[0];` |
|   2585 |  496 | `		zIn++; /* Jump the format specifer */` |
|   6925 |  497 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   6925 |  498 | `			if( c==aFmt[idx].fmttype ){` |
|   2585 |  499 | `				pInfo = &aFmt[idx];` |
|   2585 |  500 | `				xtype = pInfo->type;` |
|   2585 |  501 | `				break;` |
|      - |  502 | `			}` |
|   2175 |  503 | `		}` |
|   2585 |  504 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   2585 |  505 | `		length = 0;` |
|      - |  506 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - |  507 | `		 /*` |
|      - |  508 | `		  ** At this point, variables are initialized as follows:` |
|      - |  509 | `		  **` |
|      - |  510 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - |  511 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - |  512 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - |  513 | `		  **                               field width was negative.` |
|      - |  514 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - |  515 | `		  **                               the conversion character.` |
|      - |  516 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - |  517 | `		  **   width                       The specified field width.  This is` |
|      - |  518 | `		  **                               always non-negative.  Zero is the default.` |
|      - |  519 | `		  **   precision                   The specified precision.  The default` |
|      - |  520 | `		  **                               is -1.` |
|      - |  521 | `		  */` |
|   2585 |  522 | `		switch(xtype){` |
|      5 |  523 | `		case PH7_FMT_PERCENT:` |
|      - |  524 | `			/* A literal percent character */` |
|     11 |  525 | `			zWorker[0] = '%';` |
|     11 |  526 | `			length = (int)sizeof(char);` |
|     11 |  527 | `			break;` |
|      2 |  528 | `		case PH7_FMT_CHARX:` |
|      - |  529 | `			/* The argument is treated as an integer, and presented as the character` |
|      - |  530 | `			 * with that ASCII value` |
|      - |  531 | `			 */` |
|      5 |  532 | `			pArg = NEXT_ARG;` |
|      5 |  533 | `			if( pArg == 0 ){` |
|    ! 0 |  534 | `				c = 0;` |
|    ! 0 |  535 | `			}else{` |
|      5 |  536 | `				c = ph7_value_to_int(pArg);` |
|      - |  537 | `			}` |
|      - |  538 | `			/* NUL byte is an acceptable value */` |
|      5 |  539 | `			zWorker[0] = (char)c;` |
|      5 |  540 | `			length = (int)sizeof(char);` |
|      5 |  541 | `			break;` |
|    840 |  542 | `		case PH7_FMT_STRING:` |
|      - |  543 | `			/* the argument is treated as and presented as a string */` |
|   1685 |  544 | `			pArg = NEXT_ARG;` |
|   1685 |  545 | `			if( pArg == 0 ){` |
|    ! 0 |  546 | `				length = 0;` |
|   1685 |  547 | `			}else if( PH7_MemObjIsNotStringable(pArg) ){` |
|      - |  548 | `				/* php's user-visible coercion for %s (§2), object half: a class with` |
|      - |  549 | `				 * no __toString() is the catchable "could not be converted to` |
|      - |  550 | `				 * string" Error — but php does NOT let it interrupt the format. The` |
|      - |  551 | `				 * conversion substitutes NOTHING, the format runs to the end, the` |
|      - |  552 | `				 * output is written, and only then does the Error surface. So the` |
|      - |  553 | `				 * throw cannot be RAISED here: PHL's VmThrowException runs an` |
|      - |  554 | `				 * in-place catch immediately, which would print the format's tail` |
|      - |  555 | `				 * after the catch body. Remember the value and throw once the` |
|      - |  556 | `				 * output is out (see the tail of this function). */` |
|     21 |  557 | `				zBuf = "";` |
|     21 |  558 | `				length = 0;` |
|     21 |  559 | `				if( pThrowArg == 0 ){` |
|     19 |  560 | `					pThrowArg = pArg;` |
|      9 |  561 | `				}` |
|     11 |  562 | `			}else{` |
|      - |  563 | `				/* An ARRAY warns and renders as "Array"; a Stringable renders. */` |
|      - |  564 | `				const char *zSv;` |
|   1665 |  565 | `				sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zSv,&length);` |
|   1665 |  566 | `				zBuf = (char *)zSv;` |
|   1665 |  567 | `				if( rcSv != SXRET_OK ){` |
|      - |  568 | `					/* A __toString() that THREW: unlike the case above this one` |
|      - |  569 | `					 * cannot be predicted, and the throw has already run any` |
|      - |  570 | `					 * in-place catch. Stop formatting rather than emitting the` |
|      - |  571 | `					 * format's tail after the catch body — every other builtin that` |
|      - |  572 | `					 * calls user code (array_map, usort) stops the same way. php` |
|      - |  573 | `					 * keeps going and prints the tail; recorded divergence, and` |
|      - |  574 | `					 * both engines raise the same exception. */` |
|      3 |  575 | `					return rcSv;` |
|      - |  576 | `				}` |
|      - |  577 | `			}` |
|   1683 |  578 | `			if( length < 1 ){` |
|      - |  579 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|      - |  580 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|      - |  581 | `				 * absent optional part gained a stray space. */` |
|     30 |  582 | `				zBuf = "";` |
|     30 |  583 | `				length = 0;` |
|     14 |  584 | `			}` |
|   1683 |  585 | `			if( precision>=0 && precision<length ){` |
|      3 |  586 | `				length = precision;` |
|      1 |  587 | `			}` |
|   1683 |  588 | `			if( flag_zeropad ){` |
|      - |  589 | `				/* zero-padding works on strings too */` |
|    103 |  590 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    101 |  591 | `					spaces[idx] = '0';` |
|     51 |  592 | `				}` |
|      1 |  593 | `			}` |
|   1683 |  594 | `			break;` |
|    313 |  595 | `		case PH7_FMT_RADIX:` |
|    630 |  596 | `			pArg = NEXT_ARG;` |
|    630 |  597 | `			if( pArg == 0 ){` |
|    ! 0 |  598 | `				iVal = 0;` |
|    ! 0 |  599 | `			}else{` |
|    630 |  600 | `				iVal = ph7_value_to_int64(pArg);` |
|      - |  601 | `			}` |
|      - |  602 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    630 |  603 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 |  604 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 |  605 | `			}` |
|      - |  606 | `#if 1` |
|      - |  607 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - |  608 | `        ** I think this is stupid.*/` |
|    630 |  609 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - |  610 | `#else` |
|      - |  611 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - |  612 | `        ** but leave the prefix for hex.*/` |
|      - |  613 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - |  614 | `#endif` |
|    630 |  615 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    606 |  616 | `          if( iVal<0 ){` |
|     34 |  617 | `            iVal = -iVal;` |
|      - |  618 | `			/* Ticket 1433-003 */` |
|     34 |  619 | `			if( iVal < 0 ){` |
|      - |  620 | `				/* Overflow */` |
|    ! 0 |  621 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 |  622 | `			}` |
|     34 |  623 | `            prefix = '-';` |
|    590 |  624 | `          }else if( flag_plussign )  prefix = '+';` |
|    572 |  625 | `          else if( flag_blanksign )  prefix = ' ';` |
|    570 |  626 | `          else                       prefix = 0;` |
|    305 |  627 | `        }else{` |
|     25 |  628 | `			if( iVal<0 ){` |
|    ! 0 |  629 | `				iVal = -iVal;` |
|      - |  630 | `				/* Ticket 1433-003 */` |
|    ! 0 |  631 | `				if( iVal < 0 ){` |
|      - |  632 | `					/* Overflow */` |
|    ! 0 |  633 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 |  634 | `				}` |
|    ! 0 |  635 | `			}` |
|     25 |  636 | `			prefix = 0;` |
|      - |  637 | `		}` |
|    630 |  638 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    238 |  639 | `          precision = width-(prefix!=0);` |
|    118 |  640 | `        }` |
|    630 |  641 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - |  642 | `        {` |
|      - |  643 | `          register char *cset;      /* Use registers for speed */` |
|      - |  644 | `          register int base;` |
|    630 |  645 | `          cset = pInfo->charset;` |
|    630 |  646 | `          base = pInfo->base;` |
|    313 |  647 | `          do{                                           /* Convert to ascii */` |
|    780 |  648 | `            *(--zBuf) = cset[iVal%base];` |
|    780 |  649 | `            iVal = iVal/base;` |
|    780 |  650 | `          }while( iVal>0 );` |
|      - |  651 | `        }` |
|    630 |  652 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    850 |  653 | `        for(idx=precision-length; idx>0; idx--){` |
|    222 |  654 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|    112 |  655 | `        }` |
|    630 |  656 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    630 |  657 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - |  658 | `          char *pre, x;` |
|    ! 0 |  659 | `          pre = pInfo->prefix;` |
|    ! 0 |  660 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 |  661 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 |  662 | `          }` |
|    ! 0 |  663 | `        }` |
|    630 |  664 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    630 |  665 | `		break;` |
|    130 |  666 | `		case PH7_FMT_FLOAT:` |
|      - |  667 | `		case PH7_FMT_EXP:` |
|      - |  668 | `		case PH7_FMT_GENERIC:{` |
|      - |  669 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  670 | `		double realvalue;` |
|      - |  671 | `		char zFmt[8];` |
|      - |  672 | `		int nOut, nFmt;` |
|    265 |  673 | `		pArg = NEXT_ARG;` |
|    265 |  674 | `		if( pArg == 0 ){` |
|    ! 0 |  675 | `			realvalue = 0;` |
|    ! 0 |  676 | `		}else{` |
|    265 |  677 | `			realvalue = ph7_value_to_double(pArg);` |
|      - |  678 | `		}` |
|      - |  679 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - |  680 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    265 |  681 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 |  682 | `			zBuf = "NaN";` |
|     21 |  683 | `			length = 3;` |
|     21 |  684 | `			width = 0;` |
|     21 |  685 | `			break;` |
|      - |  686 | `		}` |
|    245 |  687 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 |  688 | `			if( realvalue < 0.0 ){` |
|     15 |  689 | `				zBuf = "-INF";` |
|     15 |  690 | `				length = 4;` |
|      8 |  691 | `			}else{` |
|     23 |  692 | `				zBuf = "INF";` |
|     23 |  693 | `				length = 3;` |
|      - |  694 | `			}` |
|     37 |  695 | `			width = 0;` |
|     37 |  696 | `			break;` |
|      - |  697 | `		}` |
|    209 |  698 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    209 |  699 | `		if( precision > 53 ){` |
|      - |  700 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - |  701 | `			 * (message prefixed with the active function's name, like` |
|      - |  702 | `			 * php_error_docref). */` |
|      - |  703 | `			char zMsg[160];` |
|      4 |  704 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  705 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 |  706 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 |  707 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 |  708 | `			precision = 53;` |
|      1 |  709 | `		}` |
|      - |  710 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - |  711 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    209 |  712 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|     20 |  713 | `			realvalue = 0.0;` |
|      9 |  714 | `		}` |
|      - |  715 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - |  716 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - |  717 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - |  718 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - |  719 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    209 |  720 | `		nFmt = 0;` |
|    209 |  721 | `		zFmt[nFmt++] = '%';` |
|    209 |  722 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - |  723 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - |  724 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    209 |  725 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    209 |  726 | `		zFmt[nFmt++] = '.';` |
|    209 |  727 | `		zFmt[nFmt++] = '*';` |
|    257 |  728 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     32 |  729 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     32 |  730 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    209 |  731 | `		zFmt[nFmt] = 0;` |
|    209 |  732 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    209 |  733 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - |  734 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - |  735 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 |  736 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 |  737 | `		}` |
|    209 |  738 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    209 |  739 | `		zBuf = zWorker;` |
|    209 |  740 | `		length = nOut;` |
|      - |  741 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - |  742 | `		 * by snprintf) and the first digit, as before. */` |
|    209 |  743 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - |  744 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - |  745 | `        ** set and we are not left justified */` |
|    209 |  746 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - |  747 | `          int i;` |
|      9 |  748 | `          int nPad = width - length;` |
|     63 |  749 | `          for(i=width; i>=nPad; i--){` |
|     55 |  750 | `            zBuf[i] = zBuf[i-nPad];` |
|     28 |  751 | `          }` |
|      9 |  752 | `          i = prefix!=0;` |
|     39 |  753 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      9 |  754 | `          length = width;` |
|      4 |  755 | `        }` |
|      - |  756 | `#else` |
|      - |  757 | `         zBuf = " ";` |
|      - |  758 | `		 length = (int)sizeof(char);` |
|      - |  759 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    209 |  760 | `		 break;` |
|      - |  761 | `							 }` |
|    ! 0 |  762 | `		default:` |
|      - |  763 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - |  764 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - |  765 | `			 * no-op that emits nothing. */` |
|    ! 0 |  766 | `			length = 0;` |
|    ! 0 |  767 | `			break;` |
|      - |  768 | `		}` |
|      - |  769 | `		 /*` |
|      - |  770 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - |  771 | `		 ** "length" characters long.The field width is "width".Do` |
|      - |  772 | `		 ** the output.` |
|      - |  773 | `		 */` |
|   2583 |  774 | `    if( !flag_leftjustify ){` |
|      - |  775 | `      register int nspace;` |
|   1805 |  776 | `      nspace = width-length;` |
|   1805 |  777 | `      if( nspace>0 ){` |
|     91 |  778 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  779 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  780 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  781 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  782 | `			}` |
|    ! 0 |  783 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  784 | `        }` |
|     91 |  785 | `        if( nspace>0 ){` |
|     91 |  786 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     91 |  787 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  788 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  789 | `			}` |
|     44 |  790 | `		}` |
|     44 |  791 | `      }` |
|    900 |  792 | `    }` |
|   2583 |  793 | `    if( length>0 ){` |
|   2555 |  794 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   2555 |  795 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  796 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  797 | `		}` |
|   1275 |  798 | `    }` |
|   2583 |  799 | `    if( flag_leftjustify ){` |
|      - |  800 | `      register int nspace;` |
|    783 |  801 | `      nspace = width-length;` |
|    783 |  802 | `      if( nspace>0 ){` |
|    615 |  803 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  804 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  805 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  806 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  807 | `			}` |
|    ! 0 |  808 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  809 | `        }` |
|    615 |  810 | `        if( nspace>0 ){` |
|    615 |  811 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    615 |  812 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  813 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  814 | `			}` |
|    305 |  815 | `		}` |
|    305 |  816 | `      }` |
|    389 |  817 | `    }` |
|      5 |  818 | ` }/* for(;;) */` |
|    997 |  819 | `	if( pThrowArg ){` |
|      - |  820 | `		/* The format ran to completion and its output is out; raise php's Error` |
|      - |  821 | ``		 * now. `printf("A[%s]B", new P())` prints "A[]B" and THEN throws, while`` |
|      - |  822 | `		 * sprintf()'s finished result is simply discarded by the unwind. */` |
|     19 |  823 | `		return PH7_MemObjToStringUV(pThrowArg);` |
|      - |  824 | `	}` |
|    979 |  825 | `	return SXRET_OK;` |
|    502 |  826 | `}` |
|      - |  827 | `/*` |
|      - |  828 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - |  829 | ` */` |
|    730 |  830 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      5 |  831 | `{` |
|      - |  832 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - |  833 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - |  834 | `	 * non-OK rc also stops the format loop. */` |
|    735 |  835 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    735 |  836 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    735 |  837 | `	return *pRc;` |
|      5 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - |  841 | ` *  Return a formatted string.` |
|      - |  842 | ` * Parameters` |
|      - |  843 | ` *  $format` |
|      - |  844 | ` *    The format string (see block comment above)` |
|      - |  845 | ` * Return` |
|      - |  846 | ` *  A string produced according to the formatting string format.` |
|      - |  847 | ` */` |
|    400 |  848 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  849 | `{` |
|      - |  850 | `	sxi32 rcFmt;` |
|      - |  851 | `	const char *zFormat;` |
|    405 |  852 | `	sxi32 rc = SXRET_OK;` |
|      - |  853 | `	int nLen;` |
|    405 |  854 | `	if( nArg < 1 ){` |
|      - |  855 | `		/* Missing arguments,return the empty string */` |
|    ! 0 |  856 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  857 | `		return PH7_OK;` |
|      - |  858 | `	}` |
|      - |  859 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    405 |  860 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    405 |  861 | `	if( rc != PH7_OK ){` |
|    ! 0 |  862 | `		return rc;` |
|      - |  863 | `	}` |
|      - |  864 | `	/* Extract the string format (scalars/null coerce). */` |
|    405 |  865 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    405 |  866 | `	if( nLen < 1 ){` |
|      - |  867 | `		/* Empty string */` |
|    ! 0 |  868 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  869 | `		return PH7_OK;` |
|      - |  870 | `	}` |
|      - |  871 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - |  872 | `	 * output; propagate the throw status verbatim. */` |
|    405 |  873 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    405 |  874 | `	if( rc != PH7_OK ){` |
|     17 |  875 | `		return rc;` |
|      - |  876 | `	}` |
|      - |  877 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|    389 |  878 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    389 |  879 | `	if( rc != PH7_OK ){` |
|     29 |  880 | `		return rc;` |
|      - |  881 | `	}` |
|      - |  882 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    361 |  883 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    361 |  884 | `	if( rc != SXRET_OK ){` |
|      - |  885 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - |  886 | `		 * returning a silently-truncated string. */` |
|    ! 0 |  887 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  888 | `	}` |
|      - |  889 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - |  890 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - |  891 | `	 * that), so report the throw last. */` |
|    361 |  892 | `	if( rcFmt != SXRET_OK ){` |
|     12 |  893 | `		pCtx->nThrowRc = rcFmt;` |
|     12 |  894 | `		return rcFmt;` |
|      - |  895 | `	}` |
|    351 |  896 | `	return PH7_OK;` |
|    205 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - |  900 | ` */` |
|   4904 |  901 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      5 |  902 | `{` |
|   4909 |  903 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - |  904 | `	/* Call the VM output consumer directly */` |
|   4909 |  905 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - |  906 | `	/* Increment counter */` |
|   4909 |  907 | `	*pCounter += nLen;` |
|   4909 |  908 | `	return PH7_OK;` |
|      5 |  909 | `}` |
|      - |  910 | `/*` |
|      - |  911 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - |  912 | ` *  Output a formatted string.` |
|      - |  913 | ` * Parameters` |
|      - |  914 | ` *  $format` |
|      - |  915 | ` *   See sprintf() for a description of format.` |
|      - |  916 | ` * Return` |
|      - |  917 | ` *  The length of the outputted string.` |
|      - |  918 | ` */` |
|    606 |  919 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  920 | `{` |
|      - |  921 | `	sxi32 rcFmt;` |
|    611 |  922 | `	ph7_int64 nCounter = 0;` |
|      - |  923 | `	const char *zFormat;` |
|      - |  924 | `	int nLen;` |
|    611 |  925 | `	if( nArg < 1 ){` |
|      - |  926 | `		/* Missing arguments,return 0 */` |
|    ! 0 |  927 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  928 | `		return PH7_OK;` |
|      - |  929 | `	}` |
|      - |  930 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - |  931 | `	{` |
|    611 |  932 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    611 |  933 | `		if( rcf != PH7_OK ){` |
|    ! 0 |  934 | `			return rcf;` |
|      - |  935 | `		}` |
|      - |  936 | `	}` |
|      - |  937 | `	/* Extract the string format (scalars/null coerce). */` |
|    611 |  938 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    611 |  939 | `	if( nLen < 1 ){` |
|      - |  940 | `		/* Empty string */` |
|    ! 0 |  941 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  942 | `		return PH7_OK;` |
|      - |  943 | `	}` |
|      - |  944 | `	{` |
|      - |  945 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|      - |  946 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|    611 |  947 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|    611 |  948 | `		if( rcv != PH7_OK ){` |
|      3 |  949 | `			return rcv;` |
|      - |  950 | `		}` |
|      - |  951 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - |  952 | `		 * before any output; propagate the throw status verbatim. */` |
|    609 |  953 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    609 |  954 | `		if( rcv != PH7_OK ){` |
|    ! 0 |  955 | `			return rcv;` |
|      - |  956 | `		}` |
|      - |  957 | `	}` |
|      - |  958 | `	/* Format the string */` |
|    609 |  959 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - |  960 | `	/* Return the length of the outputted string */` |
|    609 |  961 | `	ph7_result_int64(pCtx,nCounter);` |
|      - |  962 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - |  963 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - |  964 | `	 * that), so report the throw last. */` |
|    609 |  965 | `	if( rcFmt != SXRET_OK ){` |
|      3 |  966 | `		pCtx->nThrowRc = rcFmt;` |
|      3 |  967 | `		return rcFmt;` |
|      - |  968 | `	}` |
|    607 |  969 | `	return PH7_OK;` |
|    308 |  970 | `}` |
|      - |  971 | `/*` |
|      - |  972 | ` * int vprintf(string $format,array $args)` |
|      - |  973 | ` *  Output a formatted string.` |
|      - |  974 | ` * Parameters` |
|      - |  975 | ` *  $format` |
|      - |  976 | ` *   See sprintf() for a description of format.` |
|      - |  977 | ` * Return` |
|      - |  978 | ` *  The length of the outputted string.` |
|      - |  979 | ` */` |
|      6 |  980 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  981 | `{` |
|      - |  982 | `	sxi32 rcFmt;` |
|      8 |  983 | `	ph7_int64 nCounter = 0;` |
|      - |  984 | `	const char *zFormat;` |
|      - |  985 | `	ph7_hashmap *pMap;` |
|      - |  986 | `	SySet sArg;` |
|      - |  987 | `	int nLen,n;` |
|      8 |  988 | `	if( nArg < 2 ){` |
|      - |  989 | `		/* Missing arguments,return 0 */` |
|    ! 0 |  990 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  991 | `		return PH7_OK;` |
|      - |  992 | `	}` |
|      - |  993 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      8 |  994 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      8 |  995 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 |  996 | `		return rcFmt;` |
|      - |  997 | `	}` |
|      8 |  998 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - |  999 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 1000 | `		char zBuf[64];` |
|      4 | 1001 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1002 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 1003 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1004 | `	}` |
|      - | 1005 | `	/* Extract the string format (scalars/null coerce). */` |
|      6 | 1006 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      6 | 1007 | `	if( nLen < 1 ){` |
|      - | 1008 | `		/* Empty string */` |
|    ! 0 | 1009 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1010 | `		return PH7_OK;` |
|      - | 1011 | `	}` |
|      - | 1012 | `	/* Point to the hashmap */` |
|      6 | 1013 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 1014 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 1015 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|      - | 1016 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|      - | 1017 | `	 * rather than the missing specifier. */` |
|      6 | 1018 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      6 | 1019 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 1020 | `		return rcFmt;` |
|      - | 1021 | `	}` |
|      - | 1022 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 1023 | `	 * any output; propagate the throw status verbatim. */` |
|      6 | 1024 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      6 | 1025 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 1026 | `		return rcFmt;` |
|      - | 1027 | `	}` |
|      - | 1028 | `	/* Extract arguments from the hashmap */` |
|      6 | 1029 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 1030 | `	/* Format the string */` |
|      6 | 1031 | `	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 1032 | `	/* Release the container */` |
|      6 | 1033 | `	SySetRelease(&sArg);` |
|      - | 1034 | `	/* Return the length of the outputted string */` |
|      6 | 1035 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 1036 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1037 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1038 | `	 * that), so report the throw last. */` |
|      6 | 1039 | `	if( rcFmt != SXRET_OK ){` |
|      3 | 1040 | `		pCtx->nThrowRc = rcFmt;` |
|      3 | 1041 | `		return rcFmt;` |
|      - | 1042 | `	}` |
|      3 | 1043 | `	return PH7_OK;` |
|      5 | 1044 | `}` |
|      - | 1045 | `/*` |
|      - | 1046 | ` * int vsprintf(string $format,array $args)` |
|      - | 1047 | ` *  Output a formatted string.` |
|      - | 1048 | ` * Parameters` |
|      - | 1049 | ` *  $format` |
|      - | 1050 | ` *   See sprintf() for a description of format.` |
|      - | 1051 | ` * Return` |
|      - | 1052 | ` *  A string produced according to the formatting string format.` |
|      - | 1053 | ` */` |
|     28 | 1054 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1055 | `{` |
|      - | 1056 | `	sxi32 rcFmt;` |
|      - | 1057 | `	const char *zFormat;` |
|      - | 1058 | `	ph7_hashmap *pMap;` |
|      - | 1059 | `	SySet sArg;` |
|     30 | 1060 | `	sxi32 rc = SXRET_OK;` |
|      - | 1061 | `	int nLen,n;` |
|     30 | 1062 | `	if( nArg < 2 ){` |
|      - | 1063 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 1064 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1065 | `		return PH7_OK;` |
|      - | 1066 | `	}` |
|      - | 1067 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     30 | 1068 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     30 | 1069 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1070 | `		return rc;` |
|      - | 1071 | `	}` |
|     30 | 1072 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 1073 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 1074 | `		char zBuf[64];` |
|     13 | 1075 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1076 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      8 | 1077 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1078 | `	}` |
|      - | 1079 | `	/* Extract the string format (scalars/null coerce). */` |
|     22 | 1080 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     22 | 1081 | `	if( nLen < 1 ){` |
|      - | 1082 | `		/* Empty string */` |
|    ! 0 | 1083 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1084 | `		return PH7_OK;` |
|      - | 1085 | `	}` |
|      - | 1086 | `	/* Point to hashmap */` |
|     22 | 1087 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 1088 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 1089 | `	 * php runs this BEFORE validating the specifiers. */` |
|     22 | 1090 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     22 | 1091 | `	if( rcFmt != PH7_OK ){` |
|      7 | 1092 | `		return rcFmt;` |
|      - | 1093 | `	}` |
|      - | 1094 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 1095 | `	 * any output; propagate the throw status verbatim. */` |
|     16 | 1096 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     16 | 1097 | `	if( rcFmt != PH7_OK ){` |
|      3 | 1098 | `		return rcFmt;` |
|      - | 1099 | `	}` |
|      - | 1100 | `	/* Extract arguments from the hashmap */` |
|     14 | 1101 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 1102 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     14 | 1103 | `	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 1104 | `	/* Release the container */` |
|     14 | 1105 | `	SySetRelease(&sArg);` |
|     14 | 1106 | `	if( rc != SXRET_OK ){` |
|      - | 1107 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 1108 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1109 | `	}` |
|      - | 1110 | `	/* A %s argument that could not be coerced raised php's Error mid-format. The` |
|      - | 1111 | `	 * format still ran and the output/result still happened (php does exactly` |
|      - | 1112 | `	 * that), so report the throw last. */` |
|     14 | 1113 | `	if( rcFmt != SXRET_OK ){` |
|      3 | 1114 | `		pCtx->nThrowRc = rcFmt;` |
|      3 | 1115 | `		return rcFmt;` |
|      - | 1116 | `	}` |
|     11 | 1117 | `	return PH7_OK;` |
|     16 | 1118 | `}` |
|      - | 1119 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 1120 |  |
