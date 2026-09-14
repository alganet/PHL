# src/ph7/builtin_fmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 474/557 lines (85.10%)

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
|   502 |   94 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|     3 |   95 | `{` |
|   505 |   96 | `	const char *zEnd = &zIn[nByte];` |
|     - |   97 | `	int c,idx;` |
|  3981 |   98 | `	while( zIn < zEnd ){` |
|  3499 |   99 | `		if( zIn[0] != '%' ){` |
|  2525 |  100 | `			zIn++;` |
|  2525 |  101 | `			continue;` |
|     - |  102 | `		}` |
|   975 |  103 | `		zIn++; /* jump the percent sign */` |
|     - |  104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|     - |  105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|     - |  106 | `		 * unknown specifier, matching php. */` |
|  1215 |  107 | `		while( zIn < zEnd ){` |
|  1213 |  108 | `			c = zIn[0];` |
|  1213 |  109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   229 |  110 | `				zIn++;` |
|   229 |  111 | `				continue;` |
|     - |  112 | `			}` |
|   985 |  113 | `			if( c=='\'' ){` |
|    13 |  114 | `				zIn++;` |
|    13 |  115 | `				if( zIn < zEnd ){` |
|    13 |  116 | `					zIn++; /* the custom pad character */` |
|     6 |  117 | `				}` |
|    13 |  118 | `				continue;` |
|     - |  119 | `			}` |
|   973 |  120 | `			break;` |
|   ! 0 |  121 | `		}` |
|     - |  122 | `		/* field width */` |
|  1289 |  123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   315 |  124 | `			zIn++;` |
|     1 |  125 | `		}` |
|     - |  126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|     - |  127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|   975 |  128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    15 |  129 | `			zIn++;` |
|    17 |  130 | `			while( zIn < zEnd ){` |
|    17 |  131 | `				c = zIn[0];` |
|    17 |  132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|   ! 0 |  133 | `					zIn++;` |
|   ! 0 |  134 | `					continue;` |
|     - |  135 | `				}` |
|    17 |  136 | `				if( c=='\'' ){` |
|     3 |  137 | `					zIn++;` |
|     3 |  138 | `					if( zIn < zEnd ){` |
|     3 |  139 | `						zIn++;` |
|     1 |  140 | `					}` |
|     3 |  141 | `					continue;` |
|     - |  142 | `				}` |
|    15 |  143 | `				break;` |
|   ! 0 |  144 | `			}` |
|    23 |  145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     9 |  146 | `				zIn++;` |
|     1 |  147 | `			}` |
|     7 |  148 | `		}` |
|     - |  149 | `		/* precision */` |
|   975 |  150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   113 |  151 | `			zIn++;` |
|   243 |  152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   133 |  153 | `				zIn++;` |
|     3 |  154 | `			}` |
|    55 |  155 | `		}` |
|     - |  156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   975 |  157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|    11 |  158 | `			zIn++;` |
|     5 |  159 | `		}` |
|   975 |  160 | `		if( zIn >= zEnd ){` |
|     - |  161 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|     - |  162 | `			 * truncates here (recorded residual); nothing to validate. */` |
|     3 |  163 | `			break;` |
|     - |  164 | `		}` |
|   973 |  165 | `		c = zIn[0];` |
|   973 |  166 | `		zIn++; /* jump the conversion specifier */` |
|  3821 |  167 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|  3803 |  168 | `			if( c == aFmt[idx].fmttype ){` |
|   955 |  169 | `				break;` |
|     - |  170 | `			}` |
|  1427 |  171 | `		}` |
|   973 |  172 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|    19 |  173 | `			*pBad = c; /* unknown specifier */` |
|    19 |  174 | `			return TRUE;` |
|     - |  175 | `		}` |
|     3 |  176 | `	}` |
|   487 |  177 | `	return FALSE;` |
|   254 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|     - |  181 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|     - |  182 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|     - |  183 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|     - |  184 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|     - |  185 | ` * Returns PH7_OK when the format is valid.` |
|     - |  186 | ` */` |
|   502 |  187 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|     3 |  188 | `{` |
|   505 |  189 | `	int badSpec = 0;` |
|   505 |  190 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|    28 |  191 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     9 |  192 | `			"Unknown format specifier \"%c\"",badSpec);` |
|     - |  193 | `	}` |
|   487 |  194 | `	return PH7_OK;` |
|   254 |  195 | `}` |
|     - |  196 | `/*` |
|     - |  197 | ` * Count the number of VALUE arguments a format string needs: the greater of the` |
|     - |  198 | ` * sequential (non-positional) conversion count and the highest positional index` |
|     - |  199 | `` * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.`` |
|     - |  200 | ` */` |
|   484 |  201 | `static int FormatRequiredArgs(const char *zIn,int nByte)` |
|     3 |  202 | `{` |
|   487 |  203 | `	const char *zEnd = &zIn[nByte];` |
|   487 |  204 | `	int c,seq = 0,maxpos = 0;` |
|  3949 |  205 | `	while( zIn < zEnd ){` |
|  3467 |  206 | `		int numVal = 0,pos = 0;` |
|  3467 |  207 | `		if( zIn[0] != '%' ){` |
|  2511 |  208 | `			zIn++;` |
|  2511 |  209 | `			continue;` |
|     - |  210 | `		}` |
|   957 |  211 | `		zIn++; /* jump the percent sign */` |
|     - |  212 | `		/* leading flags (incl. the "'<pad>'" custom-pad form) */` |
|  1197 |  213 | `		while( zIn < zEnd ){` |
|  1195 |  214 | `			c = zIn[0];` |
|  1195 |  215 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|   967 |  216 | `			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|   955 |  217 | `			break;` |
|   ! 0 |  218 | `		}` |
|     - |  219 | `		/* leading number: a positional index when a '$' follows, else the width */` |
|  1271 |  220 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|   315 |  221 | `			numVal = numVal*10 + (zIn[0]-'0');` |
|   315 |  222 | `			zIn++;` |
|     1 |  223 | `		}` |
|   957 |  224 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    15 |  225 | `			pos = numVal;` |
|    15 |  226 | `			zIn++;` |
|     - |  227 | `			/* flags then width may follow the positional marker */` |
|    17 |  228 | `			while( zIn < zEnd ){` |
|    17 |  229 | `				c = zIn[0];` |
|    17 |  230 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){ zIn++; continue; }` |
|    17 |  231 | `				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }` |
|    15 |  232 | `				break;` |
|   ! 0 |  233 | `			}` |
|    23 |  234 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|     7 |  235 | `		}` |
|     - |  236 | `		/* precision */` |
|   957 |  237 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|   113 |  238 | `			zIn++;` |
|   243 |  239 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }` |
|    55 |  240 | `		}` |
|     - |  241 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|   957 |  242 | `		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }` |
|   957 |  243 | `		if( zIn >= zEnd ){ break; }` |
|   955 |  244 | `		c = zIn[0];` |
|   955 |  245 | `		zIn++; /* jump the conversion specifier */` |
|   955 |  246 | `		if( c == '%' ){ continue; } /* %% consumes no argument */` |
|   947 |  247 | `		if( pos > 0 ){` |
|    15 |  248 | `			if( pos > maxpos ){ maxpos = pos; }` |
|     8 |  249 | `		}else{` |
|   933 |  250 | `			seq++;` |
|     - |  251 | `		}` |
|     3 |  252 | `	}` |
|   487 |  253 | `	return seq > maxpos ? seq : maxpos;` |
|     3 |  254 | `}` |
|     - |  255 | `/*` |
|     - |  256 | ` * PHP 8: a printf-family call with fewer VALUE arguments than the format needs` |
|     - |  257 | ` * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises` |
|     - |  258 | ` * ArgumentCountError counting the format itself ("N arguments are required, M` |
|     - |  259 | ` * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over` |
|     - |  260 | ` * the values array ("The arguments array must contain N items, M given"). nValues` |
|     - |  261 | ` * is the count of value arguments actually supplied; nFixed is the number of` |
|     - |  262 | ` * fixed leading parameters counted in the ArgumentCountError totals (1 for the` |
|     - |  263 | ` * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg` |
|     - |  264 | ` * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK` |
|     - |  265 | ` * when enough.` |
|     - |  266 | ` */` |
|   484 |  267 | `PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)` |
|     3 |  268 | `{` |
|   487 |  269 | `	int required = FormatRequiredArgs(zFormat,nByte);` |
|   487 |  270 | `	if( nValues < required ){` |
|    21 |  271 | `		if( bVararg ){` |
|    10 |  272 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     3 |  273 | `				"The arguments array must contain %d items, %d given",required,nValues);` |
|     - |  274 | `		}` |
|    22 |  275 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     7 |  276 | `			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);` |
|     - |  277 | `	}` |
|   467 |  278 | `	return PH7_OK;` |
|   245 |  279 | `}` |
|     - |  280 | `/*` |
|     - |  281 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|     - |  282 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|     - |  283 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|     - |  284 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|     - |  285 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|     - |  286 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|     - |  287 | ` */` |
|     - |  288 | `/*` |
|     - |  289 | ` * php 8: a stream parameter that is not a resource is a TypeError, not a warning` |
|     - |  290 | ` * with a 0 return -- the caller never learned its write went nowhere.` |
|     - |  291 | ` */` |
|    24 |  292 | `PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)` |
|     1 |  293 | `{` |
|    25 |  294 | `	if( !ph7_value_is_resource(pArg) ){` |
|     - |  295 | `		char zBuf[64];` |
|     4 |  296 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  297 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|     1 |  298 | `			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|     - |  299 | `	}` |
|    23 |  300 | `	return PH7_OK;` |
|    13 |  301 | `}` |
|   514 |  302 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|     3 |  303 | `{` |
|   517 |  304 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|     - |  305 | `		char zBuf[64];` |
|   ! 0 |  306 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  307 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|   ! 0 |  308 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|     - |  309 | `	}` |
|   517 |  310 | `	return PH7_OK;` |
|   260 |  311 | `}` |
|     - |  312 | `/*` |
|     - |  313 | ` * Format a given string.` |
|     - |  314 | ` * The root program.  All variations call this core.` |
|     - |  315 | ` * INPUTS:` |
|     - |  316 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|     - |  317 | ` *            1. A pointer to the call context.` |
|     - |  318 | ` *            2. A pointer to the list of characters to be output` |
|     - |  319 | ` *               (Note, this list is NOT null terminated.)` |
|     - |  320 | ` *            3. An integer number of characters to be output.` |
|     - |  321 | ` *               (Note: This number might be zero.)` |
|     - |  322 | ` *            4. Upper layer private data.` |
|     - |  323 | ` *   zIn       This is the format string, as in the usual print.` |
|     - |  324 | ` *   apArg     This is a pointer to a list of arguments.` |
|     - |  325 | ` */` |
|   464 |  326 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|     - |  327 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|     - |  328 | `	ph7_context *pCtx,  /* call context */` |
|     - |  329 | `	const char *zIn,    /* Format string */` |
|     - |  330 | `	int nByte,          /* Format string length */` |
|     - |  331 | `	int nArg,           /* Total argument of the given arguments */` |
|     - |  332 | `	ph7_value **apArg,  /* User arguments */` |
|     - |  333 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|     - |  334 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|     - |  335 | `	)` |
|     3 |  336 | `{` |
|   467 |  337 | `	char spaces[] = "                                                  ";` |
|     - |  338 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|   467 |  339 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - |  340 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|     - |  341 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|     - |  342 | `	int flag_alternateform; /* True if "#" flag is present */` |
|     - |  343 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|     - |  344 | `	int flag_blanksign;     /* True if " " flag is present */` |
|     - |  345 | `	int flag_plussign;      /* True if "+" flag is present */` |
|     - |  346 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|     - |  347 | `	ph7_value *pArg;         /* Current processed argument */` |
|     - |  348 | `	ph7_int64 iVal;` |
|     - |  349 | `	int precision;           /* Precision of the current field */` |
|     - |  350 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|     - |  351 | `	int c,rc,n;` |
|     - |  352 | `	int length;              /* Length of the field */` |
|     - |  353 | `	int prefix;` |
|     - |  354 | `	sxu8 xtype;              /* Conversion paradigm */` |
|     - |  355 | `	int width;               /* Width of the current field */` |
|     - |  356 | `	int idx;` |
|   467 |  357 | `	n = (vf == TRUE) ? 0 : 1;` |
|     - |  358 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|     - |  359 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|     - |  360 | `	 * (called by every format builtin before this routine), so the specifier set` |
|     - |  361 | `	 * seen here is always valid. */` |
|     - |  362 | `	/* Start the format process */` |
|   692 |  363 | `	for(;;){` |
|  1387 |  364 | `		zCur = zIn;` |
|  3889 |  365 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|  2503 |  366 | `			zIn++;` |
|     1 |  367 | `		}` |
|  1387 |  368 | `		if( zCur < zIn ){` |
|     - |  369 | `			/* Consume chunk verbatim */` |
|   813 |  370 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|   813 |  371 | `			if( rc != SXRET_OK ){` |
|     - |  372 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|   ! 0 |  373 | `				break;` |
|     - |  374 | `			}` |
|   406 |  375 | `		}` |
|  1387 |  376 | `		if( zIn >= zEnd ){` |
|     - |  377 | `			/* No more input to process,break immediately */` |
|   465 |  378 | `			break;` |
|     - |  379 | `		}` |
|     - |  380 | `		/* Find out what flags are present */` |
|   925 |  381 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|   922 |  382 | `			flag_alternateform = flag_zeropad = 0;` |
|     - |  383 | `		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string` |
|     - |  384 | `		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.` |
|     - |  385 | `		 * php resets the pad character for every specifier. */` |
| 47025 |  386 | `		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }` |
|   925 |  387 | `		zIn++; /* Jump the precent sign */` |
|   461 |  388 | `		do{` |
|  1165 |  389 | `			c = zIn[0];` |
|  1165 |  390 | `			switch( c ){` |
|    19 |  391 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|     7 |  392 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|     7 |  393 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|   199 |  394 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|     6 |  395 | `			case '\'':` |
|    13 |  396 | `				zIn++;` |
|    13 |  397 | `				if( zIn < zEnd ){` |
|     - |  398 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    13 |  399 | `					c = zIn[0];` |
|   613 |  400 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   601 |  401 | `						spaces[idx] = (char)c;` |
|   301 |  402 | `					}` |
|    13 |  403 | `					c = 0;` |
|     6 |  404 | `				}` |
|    12 |  405 | `				break;` |
|   922 |  406 | `			default:                                       break;` |
|     - |  407 | `			}` |
|  1165 |  408 | `		}while( c==0 && (zIn++ < zEnd) );` |
|     - |  409 | `		/* Get the field width */` |
|   925 |  410 | `		width = 0;` |
|  1694 |  411 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   309 |  412 | `			width = width*10 + (zIn[0] - '0');` |
|   309 |  413 | `			zIn++;` |
|     1 |  414 | `		}` |
|   925 |  415 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|     - |  416 | `			/* Position specifer */` |
|     9 |  417 | `			if( width > 0 ){` |
|     9 |  418 | `				n = width;` |
|     9 |  419 | `				if( vf && n > 0 ){` |
|   ! 0 |  420 | `					n--;` |
|   ! 0 |  421 | `				}` |
|     4 |  422 | `			}` |
|     9 |  423 | `			zIn++;` |
|     9 |  424 | `			width = 0;` |
|     - |  425 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|     - |  426 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|     - |  427 | `			 * not just zero-padding. */` |
|     4 |  428 | `			do{` |
|    11 |  429 | `				c = zIn[0];` |
|    11 |  430 | `				switch( c ){` |
|   ! 0 |  431 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|   ! 0 |  432 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|   ! 0 |  433 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|   ! 0 |  434 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|     1 |  435 | `				case '\'':` |
|     3 |  436 | `					zIn++;` |
|     3 |  437 | `					if( zIn < zEnd ){` |
|     3 |  438 | `						c = zIn[0];` |
|   103 |  439 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   101 |  440 | `							spaces[idx] = (char)c;` |
|    51 |  441 | `						}` |
|     3 |  442 | `						c = 0;` |
|     1 |  443 | `					}` |
|     2 |  444 | `					break;` |
|     8 |  445 | `				default:                                       break;` |
|     - |  446 | `				}` |
|    11 |  447 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    21 |  448 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     9 |  449 | `				width = width*10 + (zIn[0] - '0');` |
|     9 |  450 | `				zIn++;` |
|     1 |  451 | `			}` |
|     4 |  452 | `		}` |
|   925 |  453 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|   ! 0 |  454 | `			width = PH7_FMT_BUFSIZ-10;` |
|   ! 0 |  455 | `		}` |
|     - |  456 | `		/* Get the precision */` |
|   925 |  457 | `		precision = -1;` |
|   925 |  458 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|   113 |  459 | `			precision = 0;` |
|   113 |  460 | `			zIn++;` |
|   298 |  461 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|   133 |  462 | `				precision = precision*10 + (zIn[0] - '0');` |
|   133 |  463 | `				zIn++;` |
|     3 |  464 | `			}` |
|    55 |  465 | `		}` |
|     - |  466 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|     - |  467 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|     - |  468 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|   925 |  469 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|     9 |  470 | `			zIn++;` |
|     4 |  471 | `		}` |
|   925 |  472 | `		if( zIn >= zEnd ){` |
|     - |  473 | `			/* No more input */` |
|     3 |  474 | `			break;` |
|     - |  475 | `		}` |
|     - |  476 | `		/* Fetch the info entry for the field */` |
|   923 |  477 | `		pInfo = 0;` |
|   923 |  478 | `		xtype = PH7_FMT_ERROR;` |
|   923 |  479 | `		c = zIn[0];` |
|   923 |  480 | `		zIn++; /* Jump the format specifer */` |
|  3451 |  481 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|  3451 |  482 | `			if( c==aFmt[idx].fmttype ){` |
|   923 |  483 | `				pInfo = &aFmt[idx];` |
|   923 |  484 | `				xtype = pInfo->type;` |
|   923 |  485 | `				break;` |
|     - |  486 | `			}` |
|  1267 |  487 | `		}` |
|   923 |  488 | `		zBuf = zWorker; /* Point to the working buffer */` |
|   923 |  489 | `		length = 0;` |
|     - |  490 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|     - |  491 | `		 /*` |
|     - |  492 | `		  ** At this point, variables are initialized as follows:` |
|     - |  493 | `		  **` |
|     - |  494 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|     - |  495 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|     - |  496 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|     - |  497 | `		  **                               field width was negative.` |
|     - |  498 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|     - |  499 | `		  **                               the conversion character.` |
|     - |  500 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|     - |  501 | `		  **   width                       The specified field width.  This is` |
|     - |  502 | `		  **                               always non-negative.  Zero is the default.` |
|     - |  503 | `		  **   precision                   The specified precision.  The default` |
|     - |  504 | `		  **                               is -1.` |
|     - |  505 | `		  */` |
|   923 |  506 | `		switch(xtype){` |
|     4 |  507 | `		case PH7_FMT_PERCENT:` |
|     - |  508 | `			/* A literal percent character */` |
|     9 |  509 | `			zWorker[0] = '%';` |
|     9 |  510 | `			length = (int)sizeof(char);` |
|     9 |  511 | `			break;` |
|     2 |  512 | `		case PH7_FMT_CHARX:` |
|     - |  513 | `			/* The argument is treated as an integer, and presented as the character` |
|     - |  514 | `			 * with that ASCII value` |
|     - |  515 | `			 */` |
|     5 |  516 | `			pArg = NEXT_ARG;` |
|     5 |  517 | `			if( pArg == 0 ){` |
|   ! 0 |  518 | `				c = 0;` |
|   ! 0 |  519 | `			}else{` |
|     5 |  520 | `				c = ph7_value_to_int(pArg);` |
|     - |  521 | `			}` |
|     - |  522 | `			/* NUL byte is an acceptable value */` |
|     5 |  523 | `			zWorker[0] = (char)c;` |
|     5 |  524 | `			length = (int)sizeof(char);` |
|     5 |  525 | `			break;` |
|   190 |  526 | `		case PH7_FMT_STRING:` |
|     - |  527 | `			/* the argument is treated as and presented as a string */` |
|   381 |  528 | `			pArg = NEXT_ARG;` |
|   381 |  529 | `			if( pArg == 0 ){` |
|   ! 0 |  530 | `				length = 0;` |
|   ! 0 |  531 | `			}else{` |
|   381 |  532 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|     - |  533 | `			}` |
|   381 |  534 | `			if( length < 1 ){` |
|     - |  535 | `				/* An empty %s substitutes NOTHING in php. PH7 substituted a single` |
|     - |  536 | `				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an` |
|     - |  537 | `				 * absent optional part gained a stray space. */` |
|     9 |  538 | `				zBuf = "";` |
|     9 |  539 | `				length = 0;` |
|     4 |  540 | `			}` |
|   381 |  541 | `			if( precision>=0 && precision<length ){` |
|     3 |  542 | `				length = precision;` |
|     1 |  543 | `			}` |
|   381 |  544 | `			if( flag_zeropad ){` |
|     - |  545 | `				/* zero-padding works on strings too */` |
|   103 |  546 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|   101 |  547 | `					spaces[idx] = '0';` |
|    51 |  548 | `				}` |
|     1 |  549 | `			}` |
|   381 |  550 | `			break;` |
|   164 |  551 | `		case PH7_FMT_RADIX:` |
|   329 |  552 | `			pArg = NEXT_ARG;` |
|   329 |  553 | `			if( pArg == 0 ){` |
|   ! 0 |  554 | `				iVal = 0;` |
|   ! 0 |  555 | `			}else{` |
|   329 |  556 | `				iVal = ph7_value_to_int64(pArg);` |
|     - |  557 | `			}` |
|     - |  558 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   329 |  559 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|   ! 0 |  560 | `				precision = PH7_FMT_BUFSIZ-40;` |
|   ! 0 |  561 | `			}` |
|     - |  562 | `#if 1` |
|     - |  563 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|     - |  564 | `        ** I think this is stupid.*/` |
|   329 |  565 | `        if( iVal==0 ) flag_alternateform = 0;` |
|     - |  566 | `#else` |
|     - |  567 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|     - |  568 | `        ** but leave the prefix for hex.*/` |
|     - |  569 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|     - |  570 | `#endif` |
|   329 |  571 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|   305 |  572 | `          if( iVal<0 ){` |
|    25 |  573 | `            iVal = -iVal;` |
|     - |  574 | `			/* Ticket 1433-003 */` |
|    25 |  575 | `			if( iVal < 0 ){` |
|     - |  576 | `				/* Overflow */` |
|   ! 0 |  577 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  578 | `			}` |
|    25 |  579 | `            prefix = '-';` |
|   293 |  580 | `          }else if( flag_plussign )  prefix = '+';` |
|   279 |  581 | `          else if( flag_blanksign )  prefix = ' ';` |
|   277 |  582 | `          else                       prefix = 0;` |
|   153 |  583 | `        }else{` |
|    25 |  584 | `			if( iVal<0 ){` |
|   ! 0 |  585 | `				iVal = -iVal;` |
|     - |  586 | `				/* Ticket 1433-003 */` |
|   ! 0 |  587 | `				if( iVal < 0 ){` |
|     - |  588 | `					/* Overflow */` |
|   ! 0 |  589 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|   ! 0 |  590 | `				}` |
|   ! 0 |  591 | `			}` |
|    25 |  592 | `			prefix = 0;` |
|     - |  593 | `		}` |
|   329 |  594 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|   185 |  595 | `          precision = width-(prefix!=0);` |
|    92 |  596 | `        }` |
|   329 |  597 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|     - |  598 | `        {` |
|     - |  599 | `          register char *cset;      /* Use registers for speed */` |
|     - |  600 | `          register int base;` |
|   329 |  601 | `          cset = pInfo->charset;` |
|   329 |  602 | `          base = pInfo->base;` |
|   164 |  603 | `          do{                                           /* Convert to ascii */` |
|   409 |  604 | `            *(--zBuf) = cset[iVal%base];` |
|   409 |  605 | `            iVal = iVal/base;` |
|   409 |  606 | `          }while( iVal>0 );` |
|     - |  607 | `        }` |
|   329 |  608 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   531 |  609 | `        for(idx=precision-length; idx>0; idx--){` |
|   203 |  610 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|   102 |  611 | `        }` |
|   329 |  612 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|   329 |  613 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|     - |  614 | `          char *pre, x;` |
|   ! 0 |  615 | `          pre = pInfo->prefix;` |
|   ! 0 |  616 | `          if( *zBuf!=pre[0] ){` |
|   ! 0 |  617 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|   ! 0 |  618 | `          }` |
|   ! 0 |  619 | `        }` |
|   329 |  620 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|   329 |  621 | `		break;` |
|   100 |  622 | `		case PH7_FMT_FLOAT:` |
|     - |  623 | `		case PH7_FMT_EXP:` |
|     - |  624 | `		case PH7_FMT_GENERIC:{` |
|     - |  625 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     - |  626 | `		double realvalue;` |
|     - |  627 | `		char zFmt[8];` |
|     - |  628 | `		int nOut, nFmt;` |
|   203 |  629 | `		pArg = NEXT_ARG;` |
|   203 |  630 | `		if( pArg == 0 ){` |
|   ! 0 |  631 | `			realvalue = 0;` |
|   ! 0 |  632 | `		}else{` |
|   203 |  633 | `			realvalue = ph7_value_to_double(pArg);` |
|     - |  634 | `		}` |
|     - |  635 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|     - |  636 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|   203 |  637 | `		if( PH7_IS_NAN(realvalue) ){` |
|    21 |  638 | `			zBuf = "NaN";` |
|    21 |  639 | `			length = 3;` |
|    21 |  640 | `			width = 0;` |
|    21 |  641 | `			break;` |
|     - |  642 | `		}` |
|   183 |  643 | `		if( PH7_IS_INF(realvalue) ){` |
|    37 |  644 | `			if( realvalue < 0.0 ){` |
|    15 |  645 | `				zBuf = "-INF";` |
|    15 |  646 | `				length = 4;` |
|     8 |  647 | `			}else{` |
|    23 |  648 | `				zBuf = "INF";` |
|    23 |  649 | `				length = 3;` |
|     - |  650 | `			}` |
|    37 |  651 | `			width = 0;` |
|    37 |  652 | `			break;` |
|     - |  653 | `		}` |
|   147 |  654 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|   147 |  655 | `		if( precision > 53 ){` |
|     - |  656 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|     - |  657 | `			 * (message prefixed with the active function's name, like` |
|     - |  658 | `			 * php_error_docref). */` |
|     - |  659 | `			char zMsg[160];` |
|     4 |  660 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     - |  661 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|     2 |  662 | `				&pCtx->pFunc->sName,precision,53);` |
|     3 |  663 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|     3 |  664 | `			precision = 53;` |
|     1 |  665 | `		}` |
|     - |  666 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|     - |  667 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|   147 |  668 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|     9 |  669 | `			realvalue = 0.0;` |
|     4 |  670 | `		}` |
|     - |  671 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|     - |  672 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|     - |  673 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|     - |  674 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|     - |  675 | `		 * expansion), then post-process into php's exact shapes below. */` |
|   147 |  676 | `		nFmt = 0;` |
|   147 |  677 | `		zFmt[nFmt++] = '%';` |
|   147 |  678 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|     - |  679 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|     - |  680 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|   147 |  681 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|   147 |  682 | `		zFmt[nFmt++] = '.';` |
|   147 |  683 | `		zFmt[nFmt++] = '*';` |
|   195 |  684 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|    32 |  685 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|    32 |  686 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|   147 |  687 | `		zFmt[nFmt] = 0;` |
|   147 |  688 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|   147 |  689 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|     - |  690 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|     - |  691 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|   ! 0 |  692 | `			nOut = (int)SyStrlen(zWorker);` |
|   ! 0 |  693 | `		}` |
|   147 |  694 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|   147 |  695 | `		zBuf = zWorker;` |
|   147 |  696 | `		length = nOut;` |
|     - |  697 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|     - |  698 | `		 * by snprintf) and the first digit, as before. */` |
|   147 |  699 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|     - |  700 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|     - |  701 | `        ** set and we are not left justified */` |
|   147 |  702 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|     - |  703 | `          int i;` |
|     9 |  704 | `          int nPad = width - length;` |
|    63 |  705 | `          for(i=width; i>=nPad; i--){` |
|    55 |  706 | `            zBuf[i] = zBuf[i-nPad];` |
|    28 |  707 | `          }` |
|     9 |  708 | `          i = prefix!=0;` |
|    39 |  709 | `          while( nPad-- ) zBuf[i++] = '0';` |
|     9 |  710 | `          length = width;` |
|     4 |  711 | `        }` |
|     - |  712 | `#else` |
|     - |  713 | `         zBuf = " ";` |
|     - |  714 | `		 length = (int)sizeof(char);` |
|     - |  715 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|   147 |  716 | `		 break;` |
|     - |  717 | `							 }` |
|   ! 0 |  718 | `		default:` |
|     - |  719 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|     - |  720 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|     - |  721 | `			 * no-op that emits nothing. */` |
|   ! 0 |  722 | `			length = 0;` |
|   ! 0 |  723 | `			break;` |
|     - |  724 | `		}` |
|     - |  725 | `		 /*` |
|     - |  726 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|     - |  727 | `		 ** "length" characters long.The field width is "width".Do` |
|     - |  728 | `		 ** the output.` |
|     - |  729 | `		 */` |
|   923 |  730 | `    if( !flag_leftjustify ){` |
|     - |  731 | `      register int nspace;` |
|   905 |  732 | `      nspace = width-length;` |
|   905 |  733 | `      if( nspace>0 ){` |
|    37 |  734 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  735 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  736 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  737 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  738 | `			}` |
|   ! 0 |  739 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  740 | `        }` |
|    37 |  741 | `        if( nspace>0 ){` |
|    37 |  742 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    37 |  743 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  744 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  745 | `			}` |
|    18 |  746 | `		}` |
|    18 |  747 | `      }` |
|   451 |  748 | `    }` |
|   923 |  749 | `    if( length>0 ){` |
|   915 |  750 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|   915 |  751 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  752 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  753 | `		}` |
|   456 |  754 | `    }` |
|   923 |  755 | `    if( flag_leftjustify ){` |
|     - |  756 | `      register int nspace;` |
|    19 |  757 | `      nspace = width-length;` |
|    19 |  758 | `      if( nspace>0 ){` |
|    15 |  759 | `        while( nspace>=etSPACESIZE ){` |
|   ! 0 |  760 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|   ! 0 |  761 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  762 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  763 | `			}` |
|   ! 0 |  764 | `			nspace -= etSPACESIZE;` |
|   ! 0 |  765 | `        }` |
|    15 |  766 | `        if( nspace>0 ){` |
|    15 |  767 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|    15 |  768 | `			if( rc != SXRET_OK ){` |
|   ! 0 |  769 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|     - |  770 | `			}` |
|     7 |  771 | `		}` |
|     7 |  772 | `      }` |
|     9 |  773 | `    }` |
|     3 |  774 | ` }/* for(;;) */` |
|   467 |  775 | `	return SXRET_OK;` |
|   235 |  776 | `}` |
|     - |  777 | `/*` |
|     - |  778 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|     - |  779 | ` */` |
|   534 |  780 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     3 |  781 | `{` |
|     - |  782 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|     - |  783 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|     - |  784 | `	 * non-OK rc also stops the format loop. */` |
|   537 |  785 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|   537 |  786 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|   537 |  787 | `	return *pRc;` |
|     3 |  788 | `}` |
|     - |  789 | `/*` |
|     - |  790 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|     - |  791 | ` *  Return a formatted string.` |
|     - |  792 | ` * Parameters` |
|     - |  793 | ` *  $format` |
|     - |  794 | ` *    The format string (see block comment above)` |
|     - |  795 | ` * Return` |
|     - |  796 | ` *  A string produced according to the formatting string format.` |
|     - |  797 | ` */` |
|   254 |  798 | `PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  799 | `{` |
|     - |  800 | `	const char *zFormat;` |
|   257 |  801 | `	sxi32 rc = SXRET_OK;` |
|     - |  802 | `	int nLen;` |
|   257 |  803 | `	if( nArg < 1 ){` |
|     - |  804 | `		/* Missing arguments,return the empty string */` |
|   ! 0 |  805 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  806 | `		return PH7_OK;` |
|     - |  807 | `	}` |
|     - |  808 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|   257 |  809 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   257 |  810 | `	if( rc != PH7_OK ){` |
|   ! 0 |  811 | `		return rc;` |
|     - |  812 | `	}` |
|     - |  813 | `	/* Extract the string format (scalars/null coerce). */` |
|   257 |  814 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   257 |  815 | `	if( nLen < 1 ){` |
|     - |  816 | `		/* Empty string */` |
|   ! 0 |  817 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  818 | `		return PH7_OK;` |
|     - |  819 | `	}` |
|     - |  820 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - |  821 | `	 * output; propagate the throw status verbatim. */` |
|   257 |  822 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   257 |  823 | `	if( rc != PH7_OK ){` |
|    17 |  824 | `		return rc;` |
|     - |  825 | `	}` |
|     - |  826 | `	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|   241 |  827 | `	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   241 |  828 | `	if( rc != PH7_OK ){` |
|    11 |  829 | `		return rc;` |
|     - |  830 | `	}` |
|     - |  831 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|   231 |  832 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|   231 |  833 | `	if( rc != SXRET_OK ){` |
|     - |  834 | `		/* The result append ran out of memory: raise a fatal rather than` |
|     - |  835 | `		 * returning a silently-truncated string. */` |
|   ! 0 |  836 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  837 | `	}` |
|   231 |  838 | `	return PH7_OK;` |
|   130 |  839 | `}` |
|     - |  840 | `/*` |
|     - |  841 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|     - |  842 | ` */` |
|  1210 |  843 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|     1 |  844 | `{` |
|  1211 |  845 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|     - |  846 | `	/* Call the VM output consumer directly */` |
|  1211 |  847 | `	ph7_context_output(pCtx,zInput,nLen);` |
|     - |  848 | `	/* Increment counter */` |
|  1211 |  849 | `	*pCounter += nLen;` |
|  1211 |  850 | `	return PH7_OK;` |
|     1 |  851 | `}` |
|     - |  852 | `/*` |
|     - |  853 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|     - |  854 | ` *  Output a formatted string.` |
|     - |  855 | ` * Parameters` |
|     - |  856 | ` *  $format` |
|     - |  857 | ` *   See sprintf() for a description of format.` |
|     - |  858 | ` * Return` |
|     - |  859 | ` *  The length of the outputted string.` |
|     - |  860 | ` */` |
|   210 |  861 | `PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  862 | `{` |
|   211 |  863 | `	ph7_int64 nCounter = 0;` |
|     - |  864 | `	const char *zFormat;` |
|     - |  865 | `	int nLen;` |
|   211 |  866 | `	if( nArg < 1 ){` |
|     - |  867 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  868 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  869 | `		return PH7_OK;` |
|     - |  870 | `	}` |
|     - |  871 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|     - |  872 | `	{` |
|   211 |  873 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|   211 |  874 | `		if( rcf != PH7_OK ){` |
|   ! 0 |  875 | `			return rcf;` |
|     - |  876 | `		}` |
|     - |  877 | `	}` |
|     - |  878 | `	/* Extract the string format (scalars/null coerce). */` |
|   211 |  879 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|   211 |  880 | `	if( nLen < 1 ){` |
|     - |  881 | `		/* Empty string */` |
|   ! 0 |  882 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  883 | `		return PH7_OK;` |
|     - |  884 | `	}` |
|     - |  885 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - |  886 | `	 * output; propagate the throw status verbatim. */` |
|     - |  887 | `	{` |
|   211 |  888 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|   211 |  889 | `		if( rcv != PH7_OK ){` |
|   ! 0 |  890 | `			return rcv;` |
|     - |  891 | `		}` |
|     - |  892 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */` |
|   211 |  893 | `		rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);` |
|   211 |  894 | `		if( rcv != PH7_OK ){` |
|     3 |  895 | `			return rcv;` |
|     - |  896 | `		}` |
|     - |  897 | `	}` |
|     - |  898 | `	/* Format the string */` |
|   209 |  899 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|     - |  900 | `	/* Return the length of the outputted string */` |
|   209 |  901 | `	ph7_result_int64(pCtx,nCounter);` |
|   209 |  902 | `	return PH7_OK;` |
|   106 |  903 | `}` |
|     - |  904 | `/*` |
|     - |  905 | ` * int vprintf(string $format,array $args)` |
|     - |  906 | ` *  Output a formatted string.` |
|     - |  907 | ` * Parameters` |
|     - |  908 | ` *  $format` |
|     - |  909 | ` *   See sprintf() for a description of format.` |
|     - |  910 | ` * Return` |
|     - |  911 | ` *  The length of the outputted string.` |
|     - |  912 | ` */` |
|     4 |  913 | `PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  914 | `{` |
|     5 |  915 | `	ph7_int64 nCounter = 0;` |
|     - |  916 | `	const char *zFormat;` |
|     - |  917 | `	ph7_hashmap *pMap;` |
|     - |  918 | `	SySet sArg;` |
|     - |  919 | `	int nLen,n;` |
|     - |  920 | `	sxi32 rcFmt;` |
|     5 |  921 | `	if( nArg < 2 ){` |
|     - |  922 | `		/* Missing arguments,return 0 */` |
|   ! 0 |  923 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  924 | `		return PH7_OK;` |
|     - |  925 | `	}` |
|     - |  926 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     5 |  927 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     5 |  928 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  929 | `		return rcFmt;` |
|     - |  930 | `	}` |
|     5 |  931 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - |  932 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - |  933 | `		char zBuf[64];` |
|     4 |  934 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  935 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     2 |  936 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - |  937 | `	}` |
|     - |  938 | `	/* Extract the string format (scalars/null coerce). */` |
|     3 |  939 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     3 |  940 | `	if( nLen < 1 ){` |
|     - |  941 | `		/* Empty string */` |
|   ! 0 |  942 | `		ph7_result_int(pCtx,0);` |
|   ! 0 |  943 | `		return PH7_OK;` |
|     - |  944 | `	}` |
|     - |  945 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - |  946 | `	 * output; propagate the throw status verbatim. */` |
|     3 |  947 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     3 |  948 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  949 | `		return rcFmt;` |
|     - |  950 | `	}` |
|     - |  951 | `	/* Point to the hashmap */` |
|     3 |  952 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - |  953 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|     - |  954 | `	 * Checked on the entry count before materialising the value set. */` |
|     3 |  955 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|     3 |  956 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 |  957 | `		return rcFmt;` |
|     - |  958 | `	}` |
|     - |  959 | `	/* Extract arguments from the hashmap */` |
|     3 |  960 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - |  961 | `	/* Format the string */` |
|     3 |  962 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|     - |  963 | `	/* Release the container */` |
|     3 |  964 | `	SySetRelease(&sArg);` |
|     - |  965 | `	/* Return the length of the outputted string */` |
|     3 |  966 | `	ph7_result_int64(pCtx,nCounter);` |
|     3 |  967 | `	return PH7_OK;` |
|     3 |  968 | `}` |
|     - |  969 | `/*` |
|     - |  970 | ` * int vsprintf(string $format,array $args)` |
|     - |  971 | ` *  Output a formatted string.` |
|     - |  972 | ` * Parameters` |
|     - |  973 | ` *  $format` |
|     - |  974 | ` *   See sprintf() for a description of format.` |
|     - |  975 | ` * Return` |
|     - |  976 | ` *  A string produced according to the formatting string format.` |
|     - |  977 | ` */` |
|    24 |  978 | `PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  979 | `{` |
|     - |  980 | `	const char *zFormat;` |
|     - |  981 | `	ph7_hashmap *pMap;` |
|     - |  982 | `	SySet sArg;` |
|    25 |  983 | `	sxi32 rc = SXRET_OK;` |
|     - |  984 | `	sxi32 rcFmt;` |
|     - |  985 | `	int nLen,n;` |
|    25 |  986 | `	if( nArg < 2 ){` |
|     - |  987 | `		/* Missing arguments,return the empty string */` |
|   ! 0 |  988 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |  989 | `		return PH7_OK;` |
|     - |  990 | `	}` |
|     - |  991 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|    25 |  992 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    25 |  993 | `	if( rc != PH7_OK ){` |
|   ! 0 |  994 | `		return rc;` |
|     - |  995 | `	}` |
|    25 |  996 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     - |  997 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|     - |  998 | `		char zBuf[64];` |
|    16 |  999 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 1000 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|    10 | 1001 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|     - | 1002 | `	}` |
|     - | 1003 | `	/* Extract the string format (scalars/null coerce). */` |
|    15 | 1004 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    15 | 1005 | `	if( nLen < 1 ){` |
|     - | 1006 | `		/* Empty string */` |
|   ! 0 | 1007 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1008 | `		return PH7_OK;` |
|     - | 1009 | `	}` |
|     - | 1010 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|     - | 1011 | `	 * output; propagate the throw status verbatim. */` |
|    15 | 1012 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    15 | 1013 | `	if( rcFmt != PH7_OK ){` |
|   ! 0 | 1014 | `		return rcFmt;` |
|     - | 1015 | `	}` |
|     - | 1016 | `	/* Point to hashmap */` |
|    15 | 1017 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - | 1018 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output. */` |
|    15 | 1019 | `	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|    15 | 1020 | `	if( rcFmt != PH7_OK ){` |
|     5 | 1021 | `		return rcFmt;` |
|     - | 1022 | `	}` |
|     - | 1023 | `	/* Extract arguments from the hashmap */` |
|    11 | 1024 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|     - | 1025 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    11 | 1026 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|     - | 1027 | `	/* Release the container */` |
|    11 | 1028 | `	SySetRelease(&sArg);` |
|    11 | 1029 | `	if( rc != SXRET_OK ){` |
|     - | 1030 | `		/* The result append ran out of memory: raise a fatal. */` |
|   ! 0 | 1031 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1032 | `	}` |
|    11 | 1033 | `	return PH7_OK;` |
|    13 | 1034 | `}` |
|     - | 1035 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1036 |  |
