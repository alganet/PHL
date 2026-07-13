# src/sx/sxfmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 279/407 lines (68.55%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "sxtypes.h"` |
|       - |    7 | `#include "sxmacros.h"` |
|       - |    8 | `#include "sxset.h"` |
|       - |    9 | `#include "sxmem.h"` |
|       - |   10 | `#include "sxfmt.h"` |
|       - |   11 | `#include "sxstr.h"` |
|       - |   12 |  |
|       - |   13 | `#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */` |
|       - |   14 | `/*` |
|       - |   15 | `** Conversion types fall into various categories as defined by the` |
|       - |   16 | `** following enumeration.` |
|       - |   17 | `*/` |
|       - |   18 | `#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|       - |   19 | `#define SXFMT_FLOAT       2 /* Floating point.%f */` |
|       - |   20 | `#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */` |
|       - |   21 | `#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|       - |   22 | `#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|       - |   23 | `#define SXFMT_STRING      6 /* Strings.%s */` |
|       - |   24 | `#define SXFMT_PERCENT     7 /* Percent symbol.%% */` |
|       - |   25 | `#define SXFMT_CHARX       8 /* Characters.%c */` |
|       - |   26 | `#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */` |
|       - |   27 | `/* Extension by Symisc Systems */` |
|       - |   28 | `#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */` |
|       - |   29 | `#define SXFMT_UNUSED     15` |
|       - |   30 | `/*` |
|       - |   31 | `** Allowed values for SyFmtInfo.flags` |
|       - |   32 | `*/` |
|       - |   33 | `#define SXFLAG_SIGNED	0x01` |
|       - |   34 | `#define SXFLAG_UNSIGNED 0x02` |
|       - |   35 | `/* Allowed values for SyFmtConsumer.nType */` |
|       - |   36 | `#define SXFMT_CONS_PROC		1	/* Consumer is a procedure */` |
|       - |   37 | `#define SXFMT_CONS_STR		2	/* Consumer is a managed string */` |
|       - |   38 | `#define SXFMT_CONS_FILE		5	/* Consumer is an open File */` |
|       - |   39 | `#define SXFMT_CONS_BLOB		6	/* Consumer is a BLOB */` |
|       - |   40 | `/*` |
|       - |   41 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|       - |   42 | `** by an instance of the following structure` |
|       - |   43 | `*/` |
|       - |   44 | `typedef struct SyFmtInfo SyFmtInfo;` |
|       - |   45 | `struct SyFmtInfo` |
|       - |   46 | `{` |
|       - |   47 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|       - |   48 | `  sxu8 base;     /* The base for radix conversion */` |
|       - |   49 | `  int flags;    /* One or more of SXFLAG_ constants below */` |
|       - |   50 | `  sxu8 type;     /* Conversion paradigm */` |
|       - |   51 | `  char *charset; /* The character set for conversion */` |
|       - |   52 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|       - |   53 | `};` |
|       - |   54 | `typedef struct SyFmtConsumer SyFmtConsumer;` |
|       - |   55 | `struct SyFmtConsumer` |
|       - |   56 | `{` |
|       - |   57 | `	sxu32 nLen; /* Total output length */` |
|       - |   58 | `	sxi32 nType; /* Type of the consumer see below */` |
|       - |   59 | `	sxi32 rc;	/* Consumer return value;Abort processing if rc != SXRET_OK */` |
|       - |   60 | ` union{` |
|       - |   61 | `	struct{` |
|       - |   62 | `	ProcConsumer xUserConsumer;` |
|       - |   63 | `	void *pUserData;` |
|       - |   64 | `	}sFunc;` |
|       - |   65 | `	SyBlob *pBlob;` |
|       - |   66 | ` }uConsumer;` |
|       - |   67 | `};` |
|       - |   68 | `/* SPDX-SnippetBegin */` |
|       - |   69 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - |   70 | `/* SPDX-License-Identifier: blessing */` |
|       - |   71 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      60 |   72 | `static int getdigit(sxlongreal *val,int *cnt)` |
|       1 |   73 | `{` |
|       - |   74 | `  sxlongreal d;` |
|       - |   75 | `  int digit;` |
|       - |   76 |  |
|      61 |   77 | `  if( (*cnt)++ >= 16 ){` |
|     ! 0 |   78 | `	  return '0';` |
|       - |   79 | `  }` |
|      61 |   80 | `  digit = (int)*val;` |
|      61 |   81 | `  d = digit;` |
|      61 |   82 | `   *val = (*val - d)*10.0;` |
|      61 |   83 | `  return digit + '0' ;` |
|      31 |   84 | `}` |
|       - |   85 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|       - |   86 | `/*` |
|       - |   87 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|       - |   88 | ` * extended by Symisc Systems to fit its need.` |
|       - |   89 | ` * Status: Public Domain` |
|       - |   90 | ` */` |
|  361204 |   91 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
|       5 |   92 | `{` |
|       - |   93 | `	/*` |
|       - |   94 | `	 * The following table is searched linearly, so it is good to put the most frequently` |
|       - |   95 | `	 * used conversion types first.` |
|       - |   96 | `	 */` |
|       - |   97 | `static const SyFmtInfo aFmt[] = {` |
|       - |   98 | `  {  'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789",0    },` |
|       - |   99 | `  {  's',  0, 0, SXFMT_STRING,     0,                  0    },` |
|       - |  100 | `  {  'c',  0, 0, SXFMT_CHARX,      0,                  0    },` |
|       - |  101 | `  {  'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },` |
|       - |  102 | `  {  'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|       - |  103 | `         /* -- Extensions by Symisc Systems -- */` |
|       - |  104 | `  {  'z',  0, 0, SXFMT_RAWSTR,     0,                   0   }, /* Pointer to a raw string (SyString *) */` |
|       - |  105 | `  {  'B',  2, 0, SXFMT_RADIX,      "01",                "b0"},` |
|       - |  106 | `         /* -- End of Extensions -- */` |
|       - |  107 | `  {  'o',  8, 0, SXFMT_RADIX,      "01234567",         "0"  },` |
|       - |  108 | `  {  'u', 10, 0, SXFMT_RADIX,      "0123456789",       0    },` |
|       - |  109 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|       - |  110 | `  {  'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0    },` |
|       - |  111 | `  {  'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0    },` |
|       - |  112 | `  {  'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0    },` |
|       - |  113 | `  {  'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0    },` |
|       - |  114 | `  {  'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0    },` |
|       - |  115 | `#endif` |
|       - |  116 | `  {  'i', 10, SXFLAG_SIGNED, SXFMT_RADIX,"0123456789", 0    },` |
|       - |  117 | `  {  'n',  0, 0, SXFMT_SIZE,       0,                  0    },` |
|       - |  118 | `  {  '%',  0, 0, SXFMT_PERCENT,    0,                  0    },` |
|       - |  119 | `  {  'p', 10, 0, SXFMT_RADIX,      "0123456789",       0    }` |
|       - |  120 | `};` |
|       - |  121 | `  int c;                     /* Next character in the format string */` |
|       - |  122 | `  char *bufpt;               /* Pointer to the conversion buffer */` |
|       - |  123 | `  int precision;             /* Precision of the current field */` |
|       - |  124 | `  int length;                /* Length of the field */` |
|       - |  125 | `  int idx;                   /* A general purpose loop counter */` |
|       - |  126 | `  int width;                 /* Width of the current field */` |
|       - |  127 | `  sxu8 flag_leftjustify;   /* True if "-" flag is present */` |
|       - |  128 | `  sxu8 flag_plussign;      /* True if "+" flag is present */` |
|       - |  129 | `  sxu8 flag_blanksign;     /* True if " " flag is present */` |
|       - |  130 | `  sxu8 flag_alternateform; /* True if "#" flag is present */` |
|       - |  131 | `  sxu8 flag_zeropad;       /* True if field width constant starts with zero */` |
|       - |  132 | `  sxu8 flag_long;          /* True if "l" flag is present */` |
|       - |  133 | `  sxi64 longvalue;         /* Value for integer types */` |
|       - |  134 | `  sxu64 ulongvalue;        /* Unsigned magnitude used for the digit conversion */` |
|       - |  135 | `  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */` |
|       - |  136 | `  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */` |
|       - |  137 | `  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/` |
|  361209 |  138 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
|       - |  139 | `  sxu8 xtype;              /* Conversion paradigm */` |
|       - |  140 | `  static char spaces[] = "                                                  ";` |
|       - |  141 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|       - |  142 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|       - |  143 | `  sxlongreal realvalue;    /* Value for real types */` |
|       - |  144 | `  int  exp;                /* exponent of real numbers */` |
|       - |  145 | `  double rounder;          /* Used for rounding floating point values */` |
|       - |  146 | `  sxu8 flag_dp;            /* True if decimal point should be shown */` |
|       - |  147 | `  sxu8 flag_rtz;           /* True if trailing zeros should be removed */` |
|       - |  148 | `  sxu8 flag_exp;           /* True to force display of the exponent */` |
|       - |  149 | `  int nsd;                 /* Number of significant digits returned */` |
|       - |  150 | `#endif` |
|       - |  151 | `  int rc;` |
|       - |  152 |  |
|  361209 |  153 | `  length = 0;` |
|  361209 |  154 | `  bufpt = 0;` |
| 1323421 |  155 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 1256335 |  156 | `    if( c!='%' ){` |
|       - |  157 | `      unsigned int amt;` |
| 1169597 |  158 | `      bufpt = (char *)zFormat;` |
| 1169597 |  159 | `      amt = 1;` |
| 1869105 |  160 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
| 1169597 |  161 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
| 1169597 |  162 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  163 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  164 | `	  }` |
| 1169597 |  165 | `      if( c==0 ){` |
|  294123 |  166 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  167 | `	  }` |
|  437737 |  168 | `    }` |
|  962217 |  169 | `    if( (c=(*++zFormat))==0 ){` |
|     ! 0 |  170 | `      errorflag = 1;` |
|     ! 0 |  171 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|     ! 0 |  172 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  173 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  174 | `	  }` |
|     ! 0 |  175 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  176 | `    }` |
|       - |  177 | `    /* Find out what flags are present */` |
|  962217 |  178 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
|  962212 |  179 | `     flag_alternateform = flag_zeropad = 0;` |
|  481106 |  180 | `    do{` |
|  962471 |  181 | `      switch( c ){` |
|     ! 0 |  182 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|       3 |  183 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  184 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      16 |  185 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     240 |  186 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
|  962212 |  187 | `        default:                                       break;` |
|       - |  188 | `      }` |
|  962471 |  189 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|       - |  190 | `    /* Get the field width */` |
|  962217 |  191 | `    width = 0;` |
|  962217 |  192 | `    if( c=='*' ){` |
|     ! 0 |  193 | `      width = va_arg(ap,int);` |
|     ! 0 |  194 | `      if( width<0 ){` |
|     ! 0 |  195 | `        flag_leftjustify = 1;` |
|     ! 0 |  196 | `        width = -width;` |
|     ! 0 |  197 | `      }` |
|     ! 0 |  198 | `      c = *++zFormat;` |
|     ! 0 |  199 | `    }else{` |
|  962537 |  200 | `      while( c>='0' && c<='9' ){` |
|     323 |  201 | `        width = width*10 + c - '0';` |
|     323 |  202 | `        c = *++zFormat;` |
|       3 |  203 | `      }` |
|       - |  204 | `    }` |
|  962217 |  205 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|     ! 0 |  206 | `      width = SXFMT_BUFSIZ-10;` |
|     ! 0 |  207 | `    }` |
|       - |  208 | `    /* Get the precision */` |
|  962217 |  209 | `	precision = -1;` |
|  962217 |  210 | `    if( c=='.' ){` |
|  290385 |  211 | `      precision = 0;` |
|  290385 |  212 | `      c = *++zFormat;` |
|  290385 |  213 | `      if( c=='*' ){` |
|  290385 |  214 | `        precision = va_arg(ap,int);` |
|  290385 |  215 | `        if( precision<0 ) precision = -precision;` |
|  290385 |  216 | `        c = *++zFormat;` |
|  145195 |  217 | `      }else{` |
|     ! 0 |  218 | `        while( c>='0' && c<='9' ){` |
|     ! 0 |  219 | `          precision = precision*10 + c - '0';` |
|     ! 0 |  220 | `          c = *++zFormat;` |
|     ! 0 |  221 | `        }` |
|       - |  222 | `      }` |
|  145190 |  223 | `    }` |
|       - |  224 | `    /* Get the conversion type modifier */` |
|  962217 |  225 | `	flag_long = 0;` |
|  962217 |  226 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|   56519 |  227 | `      flag_long = (c == 'q') ? 2 : 1;` |
|   56519 |  228 | `      c = *++zFormat;` |
|   56519 |  229 | `	  if( c == 'l' ){` |
|       - |  230 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|     ! 0 |  231 | `		  flag_long = 2;` |
|     ! 0 |  232 | `	  }` |
|   28257 |  233 | `    }` |
|       - |  234 | `    /* Fetch the info entry for the field */` |
|  962217 |  235 | `    infop = 0;` |
|  962217 |  236 | `    xtype = SXFMT_ERROR;` |
| 4285519 |  237 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 4285519 |  238 | `      if( c==aFmt[idx].fmttype ){` |
|  962217 |  239 | `        infop = &aFmt[idx];` |
|  962217 |  240 | `		xtype = infop->type;` |
|  962217 |  241 | `        break;` |
|       - |  242 | `      }` |
| 1661656 |  243 | `    }` |
|       - |  244 | `    /* zExtra is not used in this code path. */` |
|       - |  245 |  |
|       - |  246 | `    /*` |
|       - |  247 | `    ** At this point, variables are initialized as follows:` |
|       - |  248 | `    **` |
|       - |  249 | `    **   flag_alternateform          TRUE if a '#' is present.` |
|       - |  250 | `    **   flag_plussign               TRUE if a '+' is present.` |
|       - |  251 | `    **   flag_leftjustify            TRUE if a '-' is present or if the` |
|       - |  252 | `    **                               field width was negative.` |
|       - |  253 | `    **   flag_zeropad                TRUE if the width began with 0.` |
|       - |  254 | `    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed` |
|       - |  255 | `    **                               the conversion character.` |
|       - |  256 | `    **   flag_blanksign              TRUE if a ' ' is present.` |
|       - |  257 | `    **   width                       The specified field width.This is` |
|       - |  258 | `    **                               always non-negative.Zero is the default.` |
|       - |  259 | `    **   precision                   The specified precision.The default` |
|       - |  260 | `    **                               is -1.` |
|       - |  261 | `    **   xtype                       The class of the conversion.` |
|       - |  262 | `    **   infop                       Pointer to the appropriate info struct.` |
|       - |  263 | `    */` |
|  962217 |  264 | `    switch( xtype ){` |
|   30173 |  265 | `      case SXFMT_RADIX:` |
|   60351 |  266 | `        if( flag_long > 0 ){` |
|   56519 |  267 | `			if( flag_long > 1 ){` |
|       - |  268 | `				/* BSD quad: expect a 64-bit integer */` |
|   56507 |  269 | `				longvalue = va_arg(ap,sxi64);` |
|   28256 |  270 | `			}else{` |
|      13 |  271 | `				longvalue = va_arg(ap,sxlong);` |
|       - |  272 | `			}` |
|   28262 |  273 | `		}else{` |
|    3837 |  274 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|    2195 |  275 | `				longvalue = va_arg(ap,sxi32);` |
|    1100 |  276 | `			}else{` |
|    1647 |  277 | `				longvalue = va_arg(ap,sxu32);` |
|       - |  278 | `			}` |
|       - |  279 | `		}` |
|       - |  280 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   60351 |  281 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|       - |  282 | `#if 1` |
|       - |  283 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|       - |  284 | `        ** I think this is stupid.*/` |
|   60351 |  285 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|       - |  286 | `#else` |
|       - |  287 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|       - |  288 | `        ** but leave the prefix for hex.*/` |
|       - |  289 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|       - |  290 | `#endif` |
|   60351 |  291 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|   58655 |  292 | `          if( longvalue<0 ){` |
|       - |  293 | `            /* Negate in unsigned space so INT64_MIN (where -longvalue would` |
|       - |  294 | `            ** overflow, UB that recent compilers exploit) yields the correct` |
|       - |  295 | `            ** magnitude 2^63 rather than garbage. */` |
|     161 |  296 | `            ulongvalue = (sxu64)0 - (sxu64)longvalue;` |
|     161 |  297 | `            prefix = '-';` |
|      82 |  298 | `          }else{` |
|   58497 |  299 | `            ulongvalue = (sxu64)longvalue;` |
|   58497 |  300 | `            if( flag_plussign )        prefix = '+';` |
|   58495 |  301 | `            else if( flag_blanksign )  prefix = ' ';` |
|   58495 |  302 | `            else                       prefix = 0;` |
|       - |  303 | `          }` |
|   29330 |  304 | `        }else{` |
|    1701 |  305 | `			ulongvalue = (sxu64)longvalue; /* print the full unsigned value as-is */` |
|    1701 |  306 | `			prefix = 0;` |
|       - |  307 | `		}` |
|   60351 |  308 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|     240 |  309 | `          precision = width-(prefix!=0);` |
|     119 |  310 | `        }` |
|   60351 |  311 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|       - |  312 | `        {` |
|       - |  313 | `          register char *cset;      /* Use registers for speed */` |
|       - |  314 | `          register int base;` |
|   60351 |  315 | `          cset = infop->charset;` |
|   60351 |  316 | `          base = infop->base;` |
|   30173 |  317 | `          do{                                           /* Convert to ascii */` |
|  181185 |  318 | `            *(--bufpt) = cset[ulongvalue%base];` |
|  181185 |  319 | `            ulongvalue = ulongvalue/base;` |
|  181185 |  320 | `          }while( ulongvalue>0 );` |
|       - |  321 | `        }` |
|   60351 |  322 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   60766 |  323 | `        for(idx=precision-length; idx>0; idx--){` |
|     417 |  324 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|     208 |  325 | `        }` |
|   60351 |  326 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|   60351 |  327 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|       - |  328 | `          char *pre, x;` |
|       3 |  329 | `          pre = infop->prefix;` |
|       3 |  330 | `          if( *bufpt!=pre[0] ){` |
|       7 |  331 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|       1 |  332 | `          }` |
|       1 |  333 | `        }` |
|   60351 |  334 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   60351 |  335 | `        break;` |
|       5 |  336 | `      case SXFMT_FLOAT:` |
|       - |  337 | `      case SXFMT_EXP:` |
|       - |  338 | `      case SXFMT_GENERIC:` |
|       - |  339 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      11 |  340 | `		realvalue = va_arg(ap,double);` |
|       - |  341 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|      11 |  342 | `        if( PH7_IS_NAN(realvalue) ){` |
|       - |  343 | `            /* lowercase nan consistent with libc */` |
|     ! 0 |  344 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|       - |  345 | `            /* the value has no sign; make sure prefix is clear */` |
|     ! 0 |  346 | `            prefix = 0;` |
|     ! 0 |  347 | `            bufpt = buf + 3;` |
|     ! 0 |  348 | `            goto float_done;` |
|       - |  349 | `        }` |
|      11 |  350 | `        if( PH7_IS_INF(realvalue) ){` |
|     ! 0 |  351 | `            if( realvalue < 0.0 ){` |
|       - |  352 | `                /* negative infinity should be signed via prefix */` |
|     ! 0 |  353 | `                prefix = '-';` |
|     ! 0 |  354 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  355 | `                bufpt = buf + 3;` |
|     ! 0 |  356 | `            }else{` |
|       - |  357 | `                /* positive infinity treated like a plain value */` |
|     ! 0 |  358 | `                prefix = 0;` |
|     ! 0 |  359 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  360 | `                bufpt = buf + 3;` |
|       - |  361 | `            }` |
|     ! 0 |  362 | `            goto float_done;` |
|       - |  363 | `        }` |
|      11 |  364 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|      11 |  365 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|      11 |  366 | `        if( realvalue<0.0 ){` |
|       3 |  367 | `          realvalue = -realvalue;` |
|       3 |  368 | `          prefix = '-';` |
|       2 |  369 | `        }else{` |
|       9 |  370 | `          if( flag_plussign )          prefix = '+';` |
|       9 |  371 | `          else if( flag_blanksign )    prefix = ' ';` |
|       9 |  372 | `          else                         prefix = 0;` |
|       - |  373 | `        }` |
|      11 |  374 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|      11 |  375 | `        rounder = 0.0;` |
|       - |  376 | `#if 0` |
|       - |  377 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|       - |  378 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|       - |  379 | `#else` |
|       - |  380 | `        /* It makes more sense to use 0.5 */` |
|      61 |  381 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|       - |  382 | `#endif` |
|      11 |  383 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|       - |  384 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|      11 |  385 | `        exp = 0;` |
|      11 |  386 | `        if( realvalue>0.0 ){` |
|      11 |  387 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|      11 |  388 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|      11 |  389 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|      13 |  390 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|      11 |  391 | `          if( exp>350 \|\| exp<-350 ){` |
|     ! 0 |  392 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|     ! 0 |  393 | `            bufpt = buf + 3;` |
|     ! 0 |  394 | `            goto float_done;` |
|       - |  395 | `          }` |
|       5 |  396 | `        }` |
|      11 |  397 | `        bufpt = buf;` |
|       - |  398 | `        /*` |
|       - |  399 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|       - |  400 | `        ** or etFLOAT, as appropriate.` |
|       - |  401 | `        */` |
|      11 |  402 | `        flag_exp = xtype==SXFMT_EXP;` |
|      11 |  403 | `        if( xtype!=SXFMT_FLOAT ){` |
|      11 |  404 | `          realvalue += rounder;` |
|      11 |  405 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|       5 |  406 | `        }` |
|      11 |  407 | `        if( xtype==SXFMT_GENERIC ){` |
|      11 |  408 | `          flag_rtz = !flag_alternateform;` |
|      11 |  409 | `          if( exp<-4 \|\| exp>precision ){` |
|     ! 0 |  410 | `            xtype = SXFMT_EXP;` |
|     ! 0 |  411 | `          }else{` |
|      11 |  412 | `            precision = precision - exp;` |
|      11 |  413 | `            xtype = SXFMT_FLOAT;` |
|       - |  414 | `          }` |
|       6 |  415 | `        }else{` |
|     ! 0 |  416 | `          flag_rtz = 0;` |
|       - |  417 | `        }` |
|       - |  418 | `        /*` |
|       - |  419 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|       - |  420 | `        ** the precision is too large to fit in buf[].` |
|       - |  421 | `        */` |
|      11 |  422 | `        nsd = 0;` |
|      16 |  423 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|      11 |  424 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|      11 |  425 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|      11 |  426 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|      17 |  427 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|      11 |  428 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|      11 |  429 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     ! 0 |  430 | `            *(bufpt++) = '0';` |
|     ! 0 |  431 | `          }` |
|      63 |  432 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|      11 |  433 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|      11 |  434 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|      53 |  435 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|      11 |  436 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|       5 |  437 | `          }` |
|      11 |  438 | `          bufpt++;                            /* point to next free slot */` |
|       6 |  439 | `        }else{    /* etEXP or etGENERIC */` |
|     ! 0 |  440 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     ! 0 |  441 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|     ! 0 |  442 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|     ! 0 |  443 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|     ! 0 |  444 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     ! 0 |  445 | `          bufpt--;                            /* point to last digit */` |
|     ! 0 |  446 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|     ! 0 |  447 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     ! 0 |  448 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     ! 0 |  449 | `          }` |
|     ! 0 |  450 | `          bufpt++;                            /* point to next free slot */` |
|     ! 0 |  451 | `          if( exp \|\| flag_exp ){` |
|     ! 0 |  452 | `            *(bufpt++) = infop->charset[0];` |
|     ! 0 |  453 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|     ! 0 |  454 | `            else       { *(bufpt++) = '+'; }` |
|     ! 0 |  455 | `            if( exp>=100 ){` |
|     ! 0 |  456 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|     ! 0 |  457 | `              exp %= 100;` |
|     ! 0 |  458 | `            }` |
|     ! 0 |  459 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|     ! 0 |  460 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|     ! 0 |  461 | `          }` |
|       - |  462 | `        }` |
|     ! 0 |  463 | `        float_done:` |
|       - |  464 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|       - |  465 | `        ** Note that the number is in the usual order, not reversed as with` |
|       - |  466 | `        ** integer conversions.*/` |
|      11 |  467 | `        length = (int)(bufpt-buf);` |
|      11 |  468 | `        bufpt = buf;` |
|       - |  469 |  |
|       - |  470 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|       - |  471 | `        ** set and we are not left justified */` |
|      11 |  472 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|       - |  473 | `          int i;` |
|     ! 0 |  474 | `          int nPad = width - length;` |
|     ! 0 |  475 | `          for(i=width; i>=nPad; i--){` |
|     ! 0 |  476 | `            bufpt[i] = bufpt[i-nPad];` |
|     ! 0 |  477 | `          }` |
|     ! 0 |  478 | `          i = prefix!=0;` |
|     ! 0 |  479 | `          while( nPad-- ) bufpt[i++] = '0';` |
|     ! 0 |  480 | `          length = width;` |
|     ! 0 |  481 | `        }` |
|       - |  482 | `#else` |
|       - |  483 | `         bufpt = " ";` |
|       - |  484 | `		 length = (int)sizeof(" ") - 1;` |
|       - |  485 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|      11 |  486 | `        break;` |
|     ! 0 |  487 | `      case SXFMT_SIZE:{` |
|     ! 0 |  488 | `		 int *pSize = va_arg(ap,int *);` |
|     ! 0 |  489 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|     ! 0 |  490 | `		 length = width = 0;` |
|       - |  491 | `					  }` |
|     ! 0 |  492 | `        break;` |
|     ! 0 |  493 | `      case SXFMT_PERCENT:` |
|     ! 0 |  494 | `        buf[0] = '%';` |
|     ! 0 |  495 | `        bufpt = buf;` |
|     ! 0 |  496 | `        length = 1;` |
|     ! 0 |  497 | `        break;` |
|    4836 |  498 | `      case SXFMT_CHARX:` |
|    9677 |  499 | `        c = va_arg(ap,int);` |
|    9677 |  500 | `		buf[0] = (char)c;` |
|       - |  501 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    9677 |  502 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|    9677 |  503 | `        if( precision>=0 ){` |
|     ! 0 |  504 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|     ! 0 |  505 | `          length = precision;` |
|     ! 0 |  506 | `        }else{` |
|    9677 |  507 | `          length =1;` |
|       - |  508 | `        }` |
|    9677 |  509 | `        bufpt = buf;` |
|    9677 |  510 | `        break;` |
|  146284 |  511 | `      case SXFMT_STRING:` |
|  292573 |  512 | `        bufpt = va_arg(ap,char*);` |
|  292573 |  513 | `        if( bufpt==0 ){` |
|     ! 0 |  514 | `          bufpt = " ";` |
|     ! 0 |  515 | `		  length = (int)sizeof(" ")-1;` |
|     ! 0 |  516 | `		  break;` |
|       - |  517 | `        }` |
|  292573 |  518 | `		length = precision;` |
|  292573 |  519 | `		if( precision < 0 ){` |
|       - |  520 | `			/* Symisc extension */` |
|    2193 |  521 | `			length = (int)SyStrlen(bufpt);` |
|    1094 |  522 | `		}` |
|  292573 |  523 | `        if( precision>=0 && precision<length ) length = precision;` |
|  292573 |  524 | `        break;` |
|  299808 |  525 | `	case SXFMT_RAWSTR:{` |
|       - |  526 | `		/* Symisc extension */` |
|  599621 |  527 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  599621 |  528 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|     ! 0 |  529 | `			 bufpt = " ";` |
|     ! 0 |  530 | `		     length = (int)sizeof(char);` |
|     ! 0 |  531 | `		     break;` |
|       - |  532 | `		}` |
|  599621 |  533 | `		bufpt = (char *)pStr->zString;` |
|  599621 |  534 | `		length = (int)pStr->nByte;` |
|  599621 |  535 | `		break;` |
|       - |  536 | `					  }` |
|     ! 0 |  537 | `      case SXFMT_ERROR:` |
|     ! 0 |  538 | `        buf[0] = '?';` |
|     ! 0 |  539 | `        bufpt = buf;` |
|     ! 0 |  540 | `		length = (int)sizeof(char);` |
|     ! 0 |  541 | `        if( c==0 ) zFormat--;` |
|     ! 0 |  542 | `        break;` |
|       - |  543 | `    }/* End switch over the format type */` |
|       - |  544 | `    /*` |
|       - |  545 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|       - |  546 | `    ** "length" characters long.The field width is "width".Do` |
|       - |  547 | `    ** the output.` |
|       - |  548 | `    */` |
|  962217 |  549 | `    if( !flag_leftjustify ){` |
|       - |  550 | `      register int nspace;` |
|  962217 |  551 | `      nspace = width-length;` |
|  962217 |  552 | `      if( nspace>0 ){` |
|      37 |  553 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  554 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  555 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  556 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  557 | `			}` |
|     ! 0 |  558 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  559 | `        }` |
|      37 |  560 | `        if( nspace>0 ){` |
|      37 |  561 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|      37 |  562 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  563 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  564 | `			}` |
|      18 |  565 | `		}` |
|      18 |  566 | `      }` |
|  481106 |  567 | `    }` |
|  962217 |  568 | `    if( length>0 ){` |
|  962001 |  569 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
|  962001 |  570 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  571 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  572 | `		}` |
|  480998 |  573 | `    }` |
|  962217 |  574 | `    if( flag_leftjustify ){` |
|       - |  575 | `      register int nspace;` |
|     ! 0 |  576 | `      nspace = width-length;` |
|     ! 0 |  577 | `      if( nspace>0 ){` |
|     ! 0 |  578 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  579 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  580 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  581 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  582 | `			}` |
|     ! 0 |  583 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  584 | `        }` |
|     ! 0 |  585 | `        if( nspace>0 ){` |
|     ! 0 |  586 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|     ! 0 |  587 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  588 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  589 | `			}` |
|     ! 0 |  590 | `		}` |
|     ! 0 |  591 | `      }` |
|     ! 0 |  592 | `    }` |
|  481111 |  593 | `  }/* End for loop over the format string */` |
|   67091 |  594 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  180607 |  595 | `}` |
|       - |  596 | `/* SPDX-SnippetEnd */` |
| 2131624 |  597 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|       5 |  598 | `{` |
| 2131629 |  599 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 2131629 |  600 | `	sxi32 rc = SXERR_ABORT;` |
| 2131629 |  601 | `	switch(pConsumer->nType){` |
|      78 |  602 | `	case SXFMT_CONS_PROC:` |
|       - |  603 | `			/* User callback */` |
|     157 |  604 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|     157 |  605 | `			break;` |
| 1065734 |  606 | `	case SXFMT_CONS_BLOB:` |
|       - |  607 | `			/* Blob consumer */` |
| 2131473 |  608 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 2131468 |  609 | `			break;` |
|     ! 0 |  610 | `		default:` |
|       - |  611 | `			/* Unknown consumer */` |
|     ! 0 |  612 | `			break;` |
|       - |  613 | `	}` |
|       - |  614 | `	/* Update total number of bytes consumed so far */` |
| 2131629 |  615 | `	pConsumer->nLen += nLen;` |
| 2131629 |  616 | `	pConsumer->rc = rc;` |
| 2131629 |  617 | `	return rc;` |
|       5 |  618 | `}` |
|  361204 |  619 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|       5 |  620 | `{` |
|       - |  621 | `	SyFmtConsumer sCons;` |
|  361209 |  622 | `	sCons.nType = nType;` |
|  361209 |  623 | `	sCons.rc = SXRET_OK;` |
|  361209 |  624 | `	sCons.nLen = 0;` |
|  361209 |  625 | `	if( pOutLen ){` |
|   70353 |  626 | `		*pOutLen = 0;` |
|   35174 |  627 | `	}` |
|  361209 |  628 | `	switch(nType){` |
|       6 |  629 | `	case SXFMT_CONS_PROC:` |
|       - |  630 | `#if defined(UNTRUST)` |
|       - |  631 | `			if( xUserCons == 0 ){` |
|       - |  632 | `				return SXERR_EMPTY;` |
|       - |  633 | `			}` |
|       - |  634 | `#endif` |
|      13 |  635 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|      13 |  636 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|      13 |  637 | `		break;` |
|  180596 |  638 | `		case SXFMT_CONS_BLOB:` |
|  361197 |  639 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  361197 |  640 | `			break;` |
|     ! 0 |  641 | `		default:` |
|     ! 0 |  642 | `			return SXERR_UNKNOWN;` |
|       - |  643 | `	}` |
|  361209 |  644 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  361209 |  645 | `	if( pOutLen ){` |
|   70353 |  646 | `		*pOutLen = sCons.nLen;` |
|   35174 |  647 | `	}` |
|  361209 |  648 | `	return sCons.rc;` |
|  180607 |  649 | `}` |
|      12 |  650 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|       1 |  651 | `{` |
|       - |  652 | `	va_list ap;` |
|       - |  653 | `	sxi32 rc;` |
|       - |  654 | `#if defined(UNTRUST)` |
|       - |  655 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  656 | `		return SXERR_EMPTY;` |
|       - |  657 | `	}` |
|       - |  658 | `#endif` |
|      13 |  659 | `	va_start(ap,zFormat);` |
|      13 |  660 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|      13 |  661 | `	va_end(ap);` |
|      13 |  662 | `	return rc;` |
|       1 |  663 | `}` |
|   68084 |  664 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|       5 |  665 | `{` |
|       - |  666 | `	va_list ap;` |
|       - |  667 | `	sxu32 n;` |
|       - |  668 | `#if defined(UNTRUST)` |
|       - |  669 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  670 | `		return 0;` |
|       - |  671 | `	}` |
|       - |  672 | `#endif` |
|   68089 |  673 | `	va_start(ap,zFormat);` |
|   68089 |  674 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|   68089 |  675 | `	va_end(ap);` |
|   68089 |  676 | `	return n;` |
|       5 |  677 | `}` |
|    2264 |  678 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|       5 |  679 | `{` |
|    2269 |  680 | `	sxu32 n = 0; /* cc warning */` |
|       - |  681 | `#if defined(UNTRUST)` |
|       - |  682 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  683 | `		return 0;` |
|       - |  684 | `	}` |
|       - |  685 | `#endif` |
|    2269 |  686 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    2269 |  687 | `	return n;` |
|       5 |  688 | `}` |
|  290844 |  689 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|       5 |  690 | `{` |
|       - |  691 | `	SyBlob sBlob;` |
|       - |  692 | `	va_list ap;` |
|       - |  693 | `	sxu32 n;` |
|       - |  694 | `#if defined(UNTRUST)` |
|       - |  695 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  696 | `		return 0;` |
|       - |  697 | `	}` |
|       - |  698 | `#endif` |
|  290849 |  699 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|     ! 0 |  700 | `		return 0;` |
|       - |  701 | `	}` |
|  290849 |  702 | `	va_start(ap,zFormat);` |
|  290849 |  703 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  290849 |  704 | `	va_end(ap);` |
|  290849 |  705 | `	n = SyBlobLength(&sBlob);` |
|       - |  706 | `	/* Append the null terminator */` |
|  290849 |  707 | `	sBlob.mByte++;` |
|  290849 |  708 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  290849 |  709 | `	return n;` |
|  145427 |  710 | `}` |
|       - |  711 |  |
