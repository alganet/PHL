# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 485/568 lines (85.39%)

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
|   526 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)` |
|     3 |   95 | `{` |
|   529 |   96 | `	const char *zEnd = &zIn[nByte];` |
|     - |   97 | `	int c,idx;` |
|  4007 |   98 | `	while( zIn < zEnd ){` |
|  3515 |   99 | `		if( zIn[0] != '%' ){` |
|  2528 |  100 | `			zIn++;` |
|  2528 |  101 | `			continue;` |
|     - |  102 | `		}` |
|   989 |  103 | `		zIn++; /* jump the percent sign */` |
|     - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|     - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|     - |  106 | `		 * unknown specifier, matching php. */` |
|  1231 |  107 | `		while( zIn < zEnd ){` |
|  1221 |  108 | `			c = zIn[0];` |
|  1221 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   231 |  110 | `				zIn++;` |
|   231 |  111 | `				continue;` |
|     - |  112 | `			}` |
|   991 |  113 | `			if( c=='\'' ){` |
|    13 |  114 | `				zIn++;` |
|    13 |  115 | `				if( zIn < zEnd ){` |
|    13 |  116 | `					zIn++; /* the custom pad character */` |
|     6 |  117 | `				}` |
|    13 |  118 | `				continue;` |
|     - |  119 | `			}` |
|   979 |  120 | `			break;` |
|   ! 0 |  121 | `		}` |
|     - |  122 | `		/* field width */` |
|  1301 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   313 |  124 | `			zIn++;` |
|     1 |  125 | `		}` |
|     - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|     - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   989 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
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
|   989 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   137 |  151 | `			zIn++;` |
|   291 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   157 |  153 | `				zIn++;` |
|     3 |  154 | `			}` |
|    67 |  155 | `		}` |
|     - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   989 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|    11 |  158 | `			zIn++;` |
|     5 |  159 | `		}` |
|   989 |  160 | `		if( zIn >= zEnd ){` |
|     - |  161 | `			/* A dangling '%' the format string ends on: php raises` |
|     - |  162 | ``			 * `ValueError: Missing format specifier at end of string`. */`` |
|    15 |  163 | `			*pbDangling = TRUE;` |
|    15 |  164 | `			return FALSE;` |
|     - |  165 | `		}` |
|   975 |  166 | `		c = zIn[0];` |
|   975 |  167 | `		zIn++; /* jump the conversion specifier */` |
|  4071 |  168 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|  4051 |  169 | `			if( c == aFmt[idx].fmttype ){` |
|   955 |  170 | `				break;` |
|     - |  171 | `			}` |
|  1551 |  172 | `		}` |
|   975 |  173 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|    21 |  174 | `			*pBad = c; /* unknown specifier */` |
|    21 |  175 | `			return TRUE;` |
|     - |  176 | `		}` |
|     3 |  177 | `	}` |
|   495 |  178 | `	return FALSE;` |
|   266 |  179 | `}` |
|     - |  180 | `/*` |
|     - |  181 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|     - |  182 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|     - |  183 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|     - |  184 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|     - |  185 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|     - |  186 | ` * Returns PH7_OK when the format is valid.` |
|     - |  187 | ` */` |
|   526 |  188 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|     3 |  189 | `{` |
|   529 |  190 | `	int badSpec = 0,bDangling = FALSE;` |
|   529 |  191 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){` |
|    31 |  192 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    10 |  193 | `			"Unknown format specifier \"%c\"",badSpec);` |
|     - |  194 | `	}` |
|   509 |  195 | `	if( bDangling ){` |
|    15 |  196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  197 | `			"Missing format specifier at end of string");` |
|     - |  198 | `	}` |
|   495 |  199 | `	return PH7_OK;` |
|   266 |  200 | `}` |
|     - |  201 | `/*` |
|     - |  202 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|     - |  203 | ` * sequential (non-positional) conversion count and the highest positional index` |
|     - |  204 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|     - |  205 | ` */` |
|   554 |  206 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|     3 |  207 | `{` |
|   557 |  208 | `	const char *zEnd = &zIn[nByte];` |
|   557 |  209 | `	int c,seq = 0,maxpos = 0;` |
|  4131 |  210 | `	while( zIn < zEnd ){` |
|  3597 |  211 | `		int numVal = 0,pos = 0;` |
|  3597 |  212 | `		if( zIn[0] != '%' ){` |
|  2566 |  213 | `			zIn++;` |
|  2566 |  214 | `			continue;` |
|     - |  215 | `		}` |
|  1033 |  216 | `		zIn++; /* jump the percent sign */` |
|     - |  217 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|  1275 |  218 | `		while( zIn < zEnd ){` |
|  1259 |  219 | `			c = zIn[0];` |
|  1259 |  220 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|  1029 |  221 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|  1017 |  222 | `			break;` |
|   ! 0 |  223 | `		}` |
|     - |  224 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|  1351 |  225 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   319 |  226 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|   319 |  227 | `			zIn++;` |
|     1 |  228 | `		}` |
|  1033 |  229 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
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
|  1033 |  242 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   137 |  243 | `			zIn++;` |
|   291 |  244 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|    67 |  245 | `		}` |
|     - |  246 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|  1033 |  247 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|  1033 |  248 | `		if( zIn >= zEnd ){` |
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
|  1013 |  259 | `		c = zIn[0];` |
|  1013 |  260 | `		zIn++; /* jump the conversion specifier */` |
|  1013 |  261 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|  1003 |  262 | `		if( pos > 0 ){` |
|    15 |  263 | `			if( pos > maxpos ){ maxpos = pos; }` |
|     8 |  264 | `		}else{` |
|   989 |  265 | `			seq++;` |
|     - |  266 | `		}` |
|     3 |  267 | `	}` |
|   557 |  268 | `	return seq > maxpos ? seq : maxpos;` |
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
|   554 |  282 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|     3 |  283 | `{` |
|   557 |  284 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|   557 |  285 | `	if( nValues < required ){` |
|    29 |  286 | `		if( bVararg ){` |
|    13 |  287 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 |  288 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|     - |  289 | `		}` |
|    31 |  290 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    10 |  291 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|     - |  292 | `	}` |
|   529 |  293 | `	return PH7_OK;` |
|   280 |  294 | `}` |
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
|   564 |  317 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|     3 |  318 | `{` |
|   567 |  319 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|     - |  320 | `		char zBuf[64];` |
|   ! 0 |  321 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  322 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|   ! 0 |  323 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|     - |  324 | `	}` |
|   567 |  325 | `	return PH7_OK;` |
|   285 |  326 | `}` |
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
|   492 |  341 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
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
|   495 |  352 | `	char spaces[] = "                                                  ";` |
|     - |  353 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   495 |  354 | `	const char *zCur,*zEnd = &zIn[nByte];` |
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
|   495 |  372 | `	n = (vf == TRUE) ? 0 : 1;` |
|     - |  373 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|     - |  374 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|     - |  375 | `	 * (called by every format builtin before this routine), so the specifier set` |
|     - |  376 | `	 * seen here is always valid. */` |
|     - |  377 | `	/* Start the format process */` |
|   721 |  378 | `	for(;;){` |
|  1445 |  379 | `		zCur = zIn;` |
|  3945 |  380 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  2502 |  381 | `			zIn++;` |
|     2 |  382 | `		}` |
|  1445 |  383 | `		if( zCur < zIn ){` |
|     - |  384 | `			/* Consume chunk verbatim */` |
|   816 |  385 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   816 |  386 | `			if( rc != SXRET_OK ){` |
|     - |  387 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|   ! 0 |  388 | `				break;` |
|     - |  389 | `			}` |
|   407 |  390 | `		}` |
|  1445 |  391 | `		if( zIn >= zEnd ){` |
|     - |  392 | `			/* No more input to process,break immediately */` |
|   495 |  393 | `			break;` |
|     - |  394 | `		}` |
|     - |  395 | `		/* Find out what flags are present */` |
|   953 |  396 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|   950 |  397 | `			flag_alternateform = flag_zeropad = 0;` |
|     - |  398 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|     - |  399 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|     - |  400 | `		 * php resets the pad character for every specifier. */` |
| 48453 |  401 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|   953 |  402 | `		zIn++; /* Jump the precent sign */` |
|   475 |  403 | `		do{` |
|  1193 |  404 | `			c = zIn[0];` |
|  1193 |  405 | `			switch( c ){` |
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
|   950 |  421 | `			default:                                       break;` |
|     - |  422 | `			}` |
|  1193 |  423 | `		}while( c==0 && (zIn++ < zEnd) );` |
|     - |  424 | `		/* Get the field width */` |
|   953 |  425 | `		width = 0;` |
|  1736 |  426 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   309 |  427 | `			width = width*10 + (zIn[0] - '0');` |
|   309 |  428 | `			zIn++;` |
|     1 |  429 | `		}` |
|   953 |  430 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
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
|   953 |  468 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|   ! 0 |  469 | `			width = PH7_FMT_BUFSIZ-10;` |
|   ! 0 |  470 | `		}` |
|     - |  471 | `		/* Get the precision */` |
|   953 |  472 | `		precision = -1;` |
|   953 |  473 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|   137 |  474 | `			precision = 0;` |
|   137 |  475 | `			zIn++;` |
|   358 |  476 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   157 |  477 | `				precision = precision*10 + (zIn[0] - '0');` |
|   157 |  478 | `				zIn++;` |
|     3 |  479 | `			}` |
|    67 |  480 | `		}` |
|     - |  481 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|     - |  482 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|     - |  483 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   953 |  484 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|     9 |  485 | `			zIn++;` |
|     4 |  486 | `		}` |
|   953 |  487 | `		if( zIn >= zEnd ){` |
|     - |  488 | `			/* No more input */` |
|   ! 0 |  489 | `			break;` |
|     - |  490 | `		}` |
|     - |  491 | `		/* Fetch the info entry for the field */` |
|   953 |  492 | `		pInfo = 0;` |
|   953 |  493 | `		xtype = PH7_FMT_ERROR;` |
|   953 |  494 | `		c = zIn[0];` |
|   953 |  495 | `		zIn++; /* Jump the format specifer */` |
|  3709 |  496 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|  3709 |  497 | `			if( c==aFmt[idx].fmttype ){` |
|   953 |  498 | `				pInfo = &aFmt[idx];` |
|   953 |  499 | `				xtype = pInfo->type;` |
|   953 |  500 | `				break;` |
|     - |  501 | `			}` |
|  1381 |  502 | `		}` |
|   953 |  503 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   953 |  504 | `		length = 0;` |
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
|   953 |  521 | `		switch(xtype){` |
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
|   192 |  541 | `		case PH7_FMT_STRING:` |
|     - |  542 | `			/* the argument is treated as and presented as a string */` |
|   386 |  543 | `			pArg = NEXT_ARG;` |
|   386 |  544 | `			if( pArg == 0 ){` |
|   ! 0 |  545 | `				length = 0;` |
|   ! 0 |  546 | `			}else{` |
|     - |  547 | `				/* php's user-visible array->string warning for %s (§2) */` |
|   386 |  548 | `				if( pArg->iFlags & MEMOBJ_HASHMAP ){` |
|     5 |  549 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|     2 |  550 | `				}` |
|   386 |  551 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|     - |  552 | `			}` |
|   386 |  553 | `			if( length < 1 ){` |
|     - |  554 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|     - |  555 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|     - |  556 | `				 * absent optional part gained a stray space. */` |
|     9 |  557 | `				zBuf = "";` |
|     9 |  558 | `				length = 0;` |
|     4 |  559 | `			}` |
|   386 |  560 | `			if( precision>=0 && precision<length ){` |
|     3 |  561 | `				length = precision;` |
|     1 |  562 | `			}` |
|   386 |  563 | `			if( flag_zeropad ){` |
|     - |  564 | `				/* zero-padding works on strings too */` |
|   103 |  565 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   101 |  566 | `					spaces[idx] = '0';` |
|    51 |  567 | `				}` |
|     1 |  568 | `			}` |
|   386 |  569 | `			break;` |
|   164 |  570 | `		case PH7_FMT_RADIX:` |
|   329 |  571 | `			pArg = NEXT_ARG;` |
|   329 |  572 | `			if( pArg == 0 ){` |
|   ! 0 |  573 | `				iVal = 0;` |
|   ! 0 |  574 | `			}else{` |
|   329 |  575 | `				iVal = ph7_value_to_int64(pArg);` |
|     - |  576 | `			}` |
|     - |  577 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   329 |  578 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|   ! 0 |  579 | `				precision = PH7_FMT_BUFSIZ-40;` |
|   ! 0 |  580 | `			}` |
|     - |  581 | `#if 1` |
|     - |  582 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|     - |  583 | `        ** I think this is stupid.*/` |
|   329 |  584 | `        if( iVal==0 ) flag_alternateform = 0;` |
|     - |  585 | `#else` |
|     - |  586 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|     - |  587 | `        ** but leave the prefix for hex.*/` |
|     - |  588 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|     - |  589 | `#endif` |
|   329 |  590 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|   305 |  591 | `          if( iVal<0 ){` |
|    25 |  592 | `            iVal = -iVal;` |
|     - |  593 | `			/* Ticket 1433-003 */` |
|    25 |  594 | `			if( iVal < 0 ){` |
|     - |  595 | `				/* Overflow */` |
|   ! 0 |  596 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  597 | `			}` |
|    25 |  598 | `            prefix = '-';` |
|   293 |  599 | `          }else if( flag_plussign )  prefix = '+';` |
|   279 |  600 | `          else if( flag_blanksign )  prefix = ' ';` |
|   277 |  601 | `          else                       prefix = 0;` |
|   153 |  602 | `        }else{` |
|    25 |  603 | `			if( iVal<0 ){` |
|   ! 0 |  604 | `				iVal = -iVal;` |
|     - |  605 | `				/* Ticket 1433-003 */` |
|   ! 0 |  606 | `				if( iVal < 0 ){` |
|     - |  607 | `					/* Overflow */` |
|   ! 0 |  608 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  609 | `				}` |
|   ! 0 |  610 | `			}` |
|    25 |  611 | `			prefix = 0;` |
|     - |  612 | `		}` |
|   329 |  613 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|   185 |  614 | `          precision = width-(prefix!=0);` |
|    92 |  615 | `        }` |
|   329 |  616 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|     - |  617 | `        {` |
|     - |  618 | `          register char *cset;      /* Use registers for speed */` |
|     - |  619 | `          register int base;` |
|   329 |  620 | `          cset = pInfo->charset;` |
|   329 |  621 | `          base = pInfo->base;` |
|   164 |  622 | `          do{                                           /* Convert to ascii */` |
|   409 |  623 | `            *(--zBuf) = cset[iVal%base];` |
|   409 |  624 | `            iVal = iVal/base;` |
|   409 |  625 | `          }while( iVal>0 );` |
|     - |  626 | `        }` |
|   329 |  627 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   531 |  628 | `        for(idx=precision-length; idx>0; idx--){` |
|   203 |  629 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|   102 |  630 | `        }` |
|   329 |  631 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|   329 |  632 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|     - |  633 | `          char *pre, x;` |
|   ! 0 |  634 | `          pre = pInfo->prefix;` |
|   ! 0 |  635 | `          if( *zBuf!=pre[0] ){` |
|   ! 0 |  636 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|   ! 0 |  637 | `          }` |
|   ! 0 |  638 | `        }` |
|   329 |  639 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   329 |  640 | `		break;` |
|   112 |  641 | `		case PH7_FMT_FLOAT:` |
|     - |  642 | `		case PH7_FMT_EXP:` |
|     - |  643 | `		case PH7_FMT_GENERIC:{` |
|     - |  644 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     - |  645 | `		double realvalue;` |
|     - |  646 | `		char zFmt[8];` |
|     - |  647 | `		int nOut, nFmt;` |
|   227 |  648 | `		pArg = NEXT_ARG;` |
|   227 |  649 | `		if( pArg == 0 ){` |
|   ! 0 |  650 | `			realvalue = 0;` |
|   ! 0 |  651 | `		}else{` |
|   227 |  652 | `			realvalue = ph7_value_to_double(pArg);` |
|     - |  653 | `		}` |
|     - |  654 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|     - |  655 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|   227 |  656 | `		if( PH7_IS_NAN(realvalue) ){` |
|    21 |  657 | `			zBuf = "NaN";` |
|    21 |  658 | `			length = 3;` |
|    21 |  659 | `			width = 0;` |
|    21 |  660 | `			break;` |
|     - |  661 | `		}` |
|   207 |  662 | `		if( PH7_IS_INF(realvalue) ){` |
|    37 |  663 | `			if( realvalue < 0.0 ){` |
|    15 |  664 | `				zBuf = "-INF";` |
|    15 |  665 | `				length = 4;` |
|     8 |  666 | `			}else{` |
|    23 |  667 | `				zBuf = "INF";` |
|    23 |  668 | `				length = 3;` |
|     - |  669 | `			}` |
|    37 |  670 | `			width = 0;` |
|    37 |  671 | `			break;` |
|     - |  672 | `		}` |
|   171 |  673 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|   171 |  674 | `		if( precision > 53 ){` |
|     - |  675 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|     - |  676 | `			 * (message prefixed with the active function's name, like` |
|     - |  677 | `			 * php_error_docref). */` |
|     - |  678 | `			char zMsg[160];` |
|     4 |  679 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  680 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|     2 |  681 | `				&pCtx->pFunc->sName,precision,53);` |
|     3 |  682 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|     3 |  683 | `			precision = 53;` |
|     1 |  684 | `		}` |
|     - |  685 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|     - |  686 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|   171 |  687 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|    17 |  688 | `			realvalue = 0.0;` |
|     8 |  689 | `		}` |
|     - |  690 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|     - |  691 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|     - |  692 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|     - |  693 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|     - |  694 | `		 * expansion), then post-process into php's exact shapes below. */` |
|   171 |  695 | `		nFmt = 0;` |
|   171 |  696 | `		zFmt[nFmt++] = '%';` |
|   171 |  697 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|     - |  698 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|     - |  699 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|   171 |  700 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|   171 |  701 | `		zFmt[nFmt++] = '.';` |
|   171 |  702 | `		zFmt[nFmt++] = '*';` |
|   219 |  703 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|    32 |  704 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|    32 |  705 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|   171 |  706 | `		zFmt[nFmt] = 0;` |
|   171 |  707 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|   171 |  708 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|     - |  709 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|     - |  710 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|   ! 0 |  711 | `			nOut = (int)SyStrlen(zWorker);` |
|   ! 0 |  712 | `		}` |
|   171 |  713 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|   171 |  714 | `		zBuf = zWorker;` |
|   171 |  715 | `		length = nOut;` |
|     - |  716 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|     - |  717 | `		 * by snprintf) and the first digit, as before. */` |
|   171 |  718 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|     - |  719 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|     - |  720 | `        ** set and we are not left justified */` |
|   171 |  721 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|     - |  722 | `          int i;` |
|     9 |  723 | `          int nPad = width - length;` |
|    63 |  724 | `          for(i=width; i>=nPad; i--){` |
|    55 |  725 | `            zBuf[i] = zBuf[i-nPad];` |
|    28 |  726 | `          }` |
|     9 |  727 | `          i = prefix!=0;` |
|    39 |  728 | `          while( nPad-- ) zBuf[i++] = '0';` |
|     9 |  729 | `          length = width;` |
|     4 |  730 | `        }` |
|     - |  731 | `#else` |
|     - |  732 | `         zBuf = " ";` |
|     - |  733 | `		 length = (int)sizeof(char);` |
|     - |  734 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|   171 |  735 | `		 break;` |
|     - |  736 | `							 }` |
|   ! 0 |  737 | `		default:` |
|     - |  738 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|     - |  739 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|     - |  740 | `			 * no-op that emits nothing. */` |
|   ! 0 |  741 | `			length = 0;` |
|   ! 0 |  742 | `			break;` |
|     - |  743 | `		}` |
|     - |  744 | `		 /*` |
|     - |  745 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|     - |  746 | `		 ** "length" characters long.The field width is "width".Do` |
|     - |  747 | `		 ** the output.` |
|     - |  748 | `		 */` |
|   953 |  749 | `    if( !flag_leftjustify ){` |
|     - |  750 | `      register int nspace;` |
|   935 |  751 | `      nspace = width-length;` |
|   935 |  752 | `      if( nspace>0 ){` |
|    37 |  753 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  754 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  755 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  756 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  757 | `			}` |
|   ! 0 |  758 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  759 | `        }` |
|    37 |  760 | `        if( nspace>0 ){` |
|    37 |  761 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    37 |  762 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  763 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  764 | `			}` |
|    18 |  765 | `		}` |
|    18 |  766 | `      }` |
|   466 |  767 | `    }` |
|   953 |  768 | `    if( length>0 ){` |
|   945 |  769 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   945 |  770 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  771 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  772 | `		}` |
|   471 |  773 | `    }` |
|   953 |  774 | `    if( flag_leftjustify ){` |
|     - |  775 | `      register int nspace;` |
|    19 |  776 | `      nspace = width-length;` |
|    19 |  777 | `      if( nspace>0 ){` |
|    15 |  778 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  779 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  780 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  781 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  782 | `			}` |
|   ! 0 |  783 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  784 | `        }` |
|    15 |  785 | `        if( nspace>0 ){` |
|    15 |  786 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    15 |  787 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  788 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  789 | `			}` |
|     7 |  790 | `		}` |
|     7 |  791 | `      }` |
|     9 |  792 | `    }` |
|     3 |  793 | ` }/* for(;;) */` |
|   495 |  794 | `	return SXRET_OK;` |
|   249 |  795 | `}` |
|     - |  796 | `/*` |
|     - |  797 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|     - |  798 | ` */` |
|   564 |  799 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     3 |  800 | `{` |
|     - |  801 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|     - |  802 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|     - |  803 | `	 * non-OK rc also stops the format loop. */` |
|   567 |  804 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|   567 |  805 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|   567 |  806 | `	return *pRc;` |
|     3 |  807 | `}` |
|     - |  808 | `/*` |
|     - |  809 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|     - |  810 | ` *  Return a formatted string.` |
|     - |  811 | ` * Parameters` |
|     - |  812 | ` *  $format` |
|     - |  813 | ` *    The format string (see block comment above)` |
|     - |  814 | ` * Return` |
|     - |  815 | ` *  A string produced according to the formatting string format.` |
|     - |  816 | ` */` |
|   300 |  817 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  818 | `{` |
|     - |  819 | `	const char *zFormat;` |
|   303 |  820 | `	sxi32 rc = SXRET_OK;` |
|     - |  821 | `	int nLen;` |
|   303 |  822 | `	if( nArg < 1 ){` |
|     - |  823 | `		/* Missing arguments,return the empty string */` |
|   ! 0 |  824 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  825 | `		return PH7_OK;` |
|     - |  826 | `	}` |
|     - |  827 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|   303 |  828 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   303 |  829 | `	if( rc != PH7_OK ){` |
|   ! 0 |  830 | `		return rc;` |
|     - |  831 | `	}` |
|     - |  832 | `	/* Extract the string format (scalars/null coerce). */` |
|   303 |  833 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   303 |  834 | `	if( nLen < 1 ){` |
|     - |  835 | `		/* Empty string */` |
|   ! 0 |  836 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  837 | `		return PH7_OK;` |
|     - |  838 | `	}` |
|     - |  839 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - |  840 | `	 * output; propagate the throw status verbatim. */` |
|   303 |  841 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   303 |  842 | `	if( rc != PH7_OK ){` |
|    17 |  843 | `		return rc;` |
|     - |  844 | `	}` |
|     - |  845 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|   287 |  846 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   287 |  847 | `	if( rc != PH7_OK ){` |
|    29 |  848 | `		return rc;` |
|     - |  849 | `	}` |
|     - |  850 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|   259 |  851 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|   259 |  852 | `	if( rc != SXRET_OK ){` |
|     - |  853 | `		/* The result append ran out of memory: raise a fatal rather than` |
|     - |  854 | `		 * returning a silently-truncated string. */` |
|   ! 0 |  855 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  856 | `	}` |
|   259 |  857 | `	return PH7_OK;` |
|   153 |  858 | `}` |
|     - |  859 | `/*` |
|     - |  860 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|     - |  861 | ` */` |
|  1214 |  862 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     2 |  863 | `{` |
|  1216 |  864 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|     - |  865 | `	/* Call the VM output consumer directly */` |
|  1216 |  866 | `	ph7_context_output(pCtx,zInput,nLen);` |
|     - |  867 | `	/* Increment counter */` |
|  1216 |  868 | `	*pCounter += nLen;` |
|  1216 |  869 | `	return PH7_OK;` |
|     2 |  870 | `}` |
|     - |  871 | `/*` |
|     - |  872 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|     - |  873 | ` *  Output a formatted string.` |
|     - |  874 | ` * Parameters` |
|     - |  875 | ` *  $format` |
|     - |  876 | ` *   See sprintf() for a description of format.` |
|     - |  877 | ` * Return` |
|     - |  878 | ` *  The length of the outputted string.` |
|     - |  879 | ` */` |
|   212 |  880 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  881 | `{` |
|   214 |  882 | `	ph7_int64 nCounter = 0;` |
|     - |  883 | `	const char *zFormat;` |
|     - |  884 | `	int nLen;` |
|   214 |  885 | `	if( nArg < 1 ){` |
|     - |  886 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  887 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  888 | `		return PH7_OK;` |
|     - |  889 | `	}` |
|     - |  890 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|     - |  891 | `	{` |
|   214 |  892 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   214 |  893 | `		if( rcf != PH7_OK ){` |
|   ! 0 |  894 | `			return rcf;` |
|     - |  895 | `		}` |
|     - |  896 | `	}` |
|     - |  897 | `	/* Extract the string format (scalars/null coerce). */` |
|   214 |  898 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   214 |  899 | `	if( nLen < 1 ){` |
|     - |  900 | `		/* Empty string */` |
|   ! 0 |  901 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  902 | `		return PH7_OK;` |
|     - |  903 | `	}` |
|     - |  904 | `	{` |
|     - |  905 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before` |
|     - |  906 | `		 * output, and php runs this check BEFORE validating the specifiers. */` |
|   214 |  907 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   214 |  908 | `		if( rcv != PH7_OK ){` |
|     3 |  909 | `			return rcv;` |
|     - |  910 | `		}` |
|     - |  911 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|     - |  912 | `		 * before any output; propagate the throw status verbatim. */` |
|   212 |  913 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   212 |  914 | `		if( rcv != PH7_OK ){` |
|   ! 0 |  915 | `			return rcv;` |
|     - |  916 | `		}` |
|     - |  917 | `	}` |
|     - |  918 | `	/* Format the string */` |
|   212 |  919 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|     - |  920 | `	/* Return the length of the outputted string */` |
|   212 |  921 | `	ph7_result_int64(pCtx,nCounter);` |
|   212 |  922 | `	return PH7_OK;` |
|   108 |  923 | `}` |
|     - |  924 | `/*` |
|     - |  925 | ` * int vprintf(string $format,array $args)` |
|     - |  926 | ` *  Output a formatted string.` |
|     - |  927 | ` * Parameters` |
|     - |  928 | ` *  $format` |
|     - |  929 | ` *   See sprintf() for a description of format.` |
|     - |  930 | ` * Return` |
|     - |  931 | ` *  The length of the outputted string.` |
|     - |  932 | ` */` |
|     4 |  933 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  934 | `{` |
|     5 |  935 | `	ph7_int64 nCounter = 0;` |
|     - |  936 | `	const char *zFormat;` |
|     - |  937 | `	ph7_hashmap *pMap;` |
|     - |  938 | `	SySet sArg;` |
|     - |  939 | `	int nLen,n;` |
|     - |  940 | `	sxi32 rcFmt;` |
|     5 |  941 | `	if( nArg < 2 ){` |
|     - |  942 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  943 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  944 | `		return PH7_OK;` |
|     - |  945 | `	}` |
|     - |  946 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     5 |  947 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     5 |  948 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  949 | `		return rcFmt;` |
|     - |  950 | `	}` |
|     5 |  951 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - |  952 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - |  953 | `		char zBuf[64];` |
|     4 |  954 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  955 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     2 |  956 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - |  957 | `	}` |
|     - |  958 | `	/* Extract the string format (scalars/null coerce). */` |
|     3 |  959 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     3 |  960 | `	if( nLen < 1 ){` |
|     - |  961 | `		/* Empty string */` |
|   ! 0 |  962 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  963 | `		return PH7_OK;` |
|     - |  964 | `	}` |
|     - |  965 | `	/* Point to the hashmap */` |
|     3 |  966 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - |  967 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|     - |  968 | `	 * Checked on the entry count before materialising the value set. php runs this check` |
|     - |  969 | `	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item` |
|     - |  970 | `	 * rather than the missing specifier. */` |
|     3 |  971 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     3 |  972 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  973 | `		return rcFmt;` |
|     - |  974 | `	}` |
|     - |  975 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|     - |  976 | `	 * any output; propagate the throw status verbatim. */` |
|     3 |  977 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     3 |  978 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  979 | `		return rcFmt;` |
|     - |  980 | `	}` |
|     - |  981 | `	/* Extract arguments from the hashmap */` |
|     3 |  982 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - |  983 | `	/* Format the string */` |
|     3 |  984 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|     - |  985 | `	/* Release the container */` |
|     3 |  986 | `	SySetRelease(&sArg);` |
|     - |  987 | `	/* Return the length of the outputted string */` |
|     3 |  988 | `	ph7_result_int64(pCtx,nCounter);` |
|     3 |  989 | `	return PH7_OK;` |
|     3 |  990 | `}` |
|     - |  991 | `/*` |
|     - |  992 | ` * int vsprintf(string $format,array $args)` |
|     - |  993 | ` *  Output a formatted string.` |
|     - |  994 | ` * Parameters` |
|     - |  995 | ` *  $format` |
|     - |  996 | ` *   See sprintf() for a description of format.` |
|     - |  997 | ` * Return` |
|     - |  998 | ` *  A string produced according to the formatting string format.` |
|     - |  999 | ` */` |
|    26 | 1000 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1001 | `{` |
|     - | 1002 | `	const char *zFormat;` |
|     - | 1003 | `	ph7_hashmap *pMap;` |
|     - | 1004 | `	SySet sArg;` |
|    27 | 1005 | `	sxi32 rc = SXRET_OK;` |
|     - | 1006 | `	sxi32 rcFmt;` |
|     - | 1007 | `	int nLen,n;` |
|    27 | 1008 | `	if( nArg < 2 ){` |
|     - | 1009 | `		/* Missing arguments,return the empty string */` |
|   ! 0 | 1010 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1011 | `		return PH7_OK;` |
|     - | 1012 | `	}` |
|     - | 1013 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|    27 | 1014 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    27 | 1015 | `	if( rc != PH7_OK ){` |
|   ! 0 | 1016 | `		return rc;` |
|     - | 1017 | `	}` |
|    27 | 1018 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - | 1019 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - | 1020 | `		char zBuf[64];` |
|    13 | 1021 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1022 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     8 | 1023 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - | 1024 | `	}` |
|     - | 1025 | `	/* Extract the string format (scalars/null coerce). */` |
|    19 | 1026 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1027 | `	if( nLen < 1 ){` |
|     - | 1028 | `		/* Empty string */` |
|   ! 0 | 1029 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1030 | `		return PH7_OK;` |
|     - | 1031 | `	}` |
|     - | 1032 | `	/* Point to hashmap */` |
|    19 | 1033 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - | 1034 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|     - | 1035 | `	 * php runs this BEFORE validating the specifiers. */` |
|    19 | 1036 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|    19 | 1037 | `	if( rcFmt != PH7_OK ){` |
|     7 | 1038 | `		return rcFmt;` |
|     - | 1039 | `	}` |
|     - | 1040 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|     - | 1041 | `	 * any output; propagate the throw status verbatim. */` |
|    13 | 1042 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    13 | 1043 | `	if( rcFmt != PH7_OK ){` |
|     3 | 1044 | `		return rcFmt;` |
|     - | 1045 | `	}` |
|     - | 1046 | `	/* Extract arguments from the hashmap */` |
|    11 | 1047 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - | 1048 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    11 | 1049 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|     - | 1050 | `	/* Release the container */` |
|    11 | 1051 | `	SySetRelease(&sArg);` |
|    11 | 1052 | `	if( rc != SXRET_OK ){` |
|     - | 1053 | `		/* The result append ran out of memory: raise a fatal. */` |
|   ! 0 | 1054 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1055 | `	}` |
|    11 | 1056 | `	return PH7_OK;` |
|    14 | 1057 | `}` |
|     - | 1058 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1059 |  |
