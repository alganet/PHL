# src/sx/sxfmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 216/411 lines (52.55%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "sxtypes.h"` |
|        - |    7 | `#include "sxmacros.h"` |
|        - |    8 | `#include "sxset.h"` |
|        - |    9 | `#include "sxmem.h"` |
|        - |   10 | `#include "sxfmt.h"` |
|        - |   11 | `#include "sxstr.h"` |
|        - |   12 |  |
|        - |   13 | `#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */` |
|        - |   14 | `/*` |
|        - |   15 | `** Conversion types fall into various categories as defined by the` |
|        - |   16 | `** following enumeration.` |
|        - |   17 | `*/` |
|        - |   18 | `#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|        - |   19 | `#define SXFMT_FLOAT       2 /* Floating point.%f */` |
|        - |   20 | `#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */` |
|        - |   21 | `#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|        - |   22 | `#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|        - |   23 | `#define SXFMT_STRING      6 /* Strings.%s */` |
|        - |   24 | `#define SXFMT_PERCENT     7 /* Percent symbol.%% */` |
|        - |   25 | `#define SXFMT_CHARX       8 /* Characters.%c */` |
|        - |   26 | `#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */` |
|        - |   27 | `/* Extension by Symisc Systems */` |
|        - |   28 | `#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */` |
|        - |   29 | `#define SXFMT_UNUSED     15` |
|        - |   30 | `/*` |
|        - |   31 | `** Allowed values for SyFmtInfo.flags` |
|        - |   32 | `*/` |
|        - |   33 | `#define SXFLAG_SIGNED	0x01` |
|        - |   34 | `#define SXFLAG_UNSIGNED 0x02` |
|        - |   35 | `/* Allowed values for SyFmtConsumer.nType */` |
|        - |   36 | `#define SXFMT_CONS_PROC		1	/* Consumer is a procedure */` |
|        - |   37 | `#define SXFMT_CONS_STR		2	/* Consumer is a managed string */` |
|        - |   38 | `#define SXFMT_CONS_FILE		5	/* Consumer is an open File */` |
|        - |   39 | `#define SXFMT_CONS_BLOB		6	/* Consumer is a BLOB */` |
|        - |   40 | `/*` |
|        - |   41 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|        - |   42 | `** by an instance of the following structure` |
|        - |   43 | `*/` |
|        - |   44 | `typedef struct SyFmtInfo SyFmtInfo;` |
|        - |   45 | `struct SyFmtInfo` |
|        - |   46 | `{` |
|        - |   47 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|        - |   48 | `  sxu8 base;     /* The base for radix conversion */` |
|        - |   49 | `  int flags;    /* One or more of SXFLAG_ constants below */` |
|        - |   50 | `  sxu8 type;     /* Conversion paradigm */` |
|        - |   51 | `  char *charset; /* The character set for conversion */` |
|        - |   52 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|        - |   53 | `};` |
|        - |   54 | `typedef struct SyFmtConsumer SyFmtConsumer;` |
|        - |   55 | `struct SyFmtConsumer` |
|        - |   56 | `{` |
|        - |   57 | `	sxu32 nLen; /* Total output length */` |
|        - |   58 | `	sxi32 nType; /* Type of the consumer see below */` |
|        - |   59 | `	sxi32 rc;	/* Consumer return value;Abort processing if rc != SXRET_OK */` |
|        - |   60 | ` union{` |
|        - |   61 | `	struct{` |
|        - |   62 | `	ProcConsumer xUserConsumer;` |
|        - |   63 | `	void *pUserData;` |
|        - |   64 | `	}sFunc;` |
|        - |   65 | `	SyBlob *pBlob;` |
|        - |   66 | ` }uConsumer;` |
|        - |   67 | `};` |
|        - |   68 | `/* SPDX-SnippetBegin */` |
|        - |   69 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|        - |   70 | `/* SPDX-License-Identifier: blessing */` |
|        - |   71 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      ! 0 |   72 | `static int getdigit(sxlongreal *val,int *cnt)` |
|      ! 0 |   73 | `{` |
|        - |   74 | `  sxlongreal d;` |
|        - |   75 | `  int digit;` |
|        - |   76 |  |
|      ! 0 |   77 | `  if( (*cnt)++ >= 16 ){` |
|      ! 0 |   78 | `	  return '0';` |
|        - |   79 | `  }` |
|      ! 0 |   80 | `  digit = (int)*val;` |
|      ! 0 |   81 | `  d = digit;` |
|      ! 0 |   82 | `   *val = (*val - d)*10.0;` |
|      ! 0 |   83 | `  return digit + '0' ;` |
|      ! 0 |   84 | `}` |
|        - |   85 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|        - |   86 | `/*` |
|        - |   87 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|        - |   88 | ` * extended by Symisc Systems to fit its need.` |
|        - |   89 | ` * Status: Public Domain` |
|        - |   90 | ` */` |
|  7762740 |   91 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
|        5 |   92 | `{` |
|        - |   93 | `	/*` |
|        - |   94 | `	 * The following table is searched linearly, so it is good to put the most frequently` |
|        - |   95 | `	 * used conversion types first.` |
|        - |   96 | `	 */` |
|        - |   97 | `static const SyFmtInfo aFmt[] = {` |
|        - |   98 | `  {  'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789",0    },` |
|        - |   99 | `  {  's',  0, 0, SXFMT_STRING,     0,                  0    },` |
|        - |  100 | `  {  'c',  0, 0, SXFMT_CHARX,      0,                  0    },` |
|        - |  101 | `  {  'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },` |
|        - |  102 | `  {  'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|        - |  103 | `         /* -- Extensions by Symisc Systems -- */` |
|        - |  104 | `  {  'z',  0, 0, SXFMT_RAWSTR,     0,                   0   }, /* Pointer to a raw string (SyString *) */` |
|        - |  105 | `  {  'B',  2, 0, SXFMT_RADIX,      "01",                "b0"},` |
|        - |  106 | `         /* -- End of Extensions -- */` |
|        - |  107 | `  {  'o',  8, 0, SXFMT_RADIX,      "01234567",         "0"  },` |
|        - |  108 | `  {  'u', 10, 0, SXFMT_RADIX,      "0123456789",       0    },` |
|        - |  109 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|        - |  110 | `  {  'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0    },` |
|        - |  111 | `  {  'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0    },` |
|        - |  112 | `  {  'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0    },` |
|        - |  113 | `  {  'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0    },` |
|        - |  114 | `  {  'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0    },` |
|        - |  115 | `#endif` |
|        - |  116 | `  {  'i', 10, SXFLAG_SIGNED, SXFMT_RADIX,"0123456789", 0    },` |
|        - |  117 | `  {  'n',  0, 0, SXFMT_SIZE,       0,                  0    },` |
|        - |  118 | `  {  '%',  0, 0, SXFMT_PERCENT,    0,                  0    },` |
|        - |  119 | `  {  'p', 10, 0, SXFMT_RADIX,      "0123456789",       0    }` |
|        - |  120 | `};` |
|        - |  121 | `  int c;                     /* Next character in the format string */` |
|        - |  122 | `  char *bufpt;               /* Pointer to the conversion buffer */` |
|        - |  123 | `  int precision;             /* Precision of the current field */` |
|        - |  124 | `  int length;                /* Length of the field */` |
|        - |  125 | `  int idx;                   /* A general purpose loop counter */` |
|        - |  126 | `  int width;                 /* Width of the current field */` |
|        - |  127 | `  sxu8 flag_leftjustify;   /* True if "-" flag is present */` |
|        - |  128 | `  sxu8 flag_plussign;      /* True if "+" flag is present */` |
|        - |  129 | `  sxu8 flag_blanksign;     /* True if " " flag is present */` |
|        - |  130 | `  sxu8 flag_alternateform; /* True if "#" flag is present */` |
|        - |  131 | `  sxu8 flag_zeropad;       /* True if field width constant starts with zero */` |
|        - |  132 | `  sxu8 flag_long;          /* True if "l" flag is present */` |
|        - |  133 | `  sxi64 longvalue;         /* Value for integer types */` |
|        - |  134 | `  sxu64 ulongvalue;        /* Unsigned magnitude used for the digit conversion */` |
|        - |  135 | `  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */` |
|        - |  136 | `  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */` |
|        - |  137 | `  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/` |
|  7762745 |  138 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
|        - |  139 | `  sxu8 xtype;              /* Conversion paradigm */` |
|        - |  140 | `  static char spaces[] = "                                                  ";` |
|        - |  141 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|        - |  142 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|        - |  143 | `  sxlongreal realvalue;    /* Value for real types */` |
|        - |  144 | `  int  exp;                /* exponent of real numbers */` |
|        - |  145 | `  double rounder;          /* Used for rounding floating point values */` |
|        - |  146 | `  sxu8 flag_dp;            /* True if decimal point should be shown */` |
|        - |  147 | `  sxu8 flag_rtz;           /* True if trailing zeros should be removed */` |
|        - |  148 | `  sxu8 flag_exp;           /* True to force display of the exponent */` |
|        - |  149 | `  int nsd;                 /* Number of significant digits returned */` |
|        - |  150 | `#endif` |
|        - |  151 | `  int rc;` |
|        - |  152 |  |
|  7762745 |  153 | `  length = 0;` |
|  7762745 |  154 | `  bufpt = 0;` |
| 26953699 |  155 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 23241795 |  156 | `    if( c!='%' ){` |
|        - |  157 | `      unsigned int amt;` |
| 19602191 |  158 | `      bufpt = (char *)zFormat;` |
| 19602191 |  159 | `      amt = 1;` |
| 46497909 |  160 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
| 19602191 |  161 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
| 19602191 |  162 | `	  if( rc != SXRET_OK ){` |
|      ! 0 |  163 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  164 | `	  }` |
| 19602191 |  165 | `      if( c==0 ){` |
|  4050841 |  166 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|        - |  167 | `	  }` |
|  7775675 |  168 | `    }` |
| 19190959 |  169 | `    if( (c=(*++zFormat))==0 ){` |
|      ! 0 |  170 | `      errorflag = 1;` |
|      ! 0 |  171 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|      ! 0 |  172 | `	  if( rc != SXRET_OK ){` |
|      ! 0 |  173 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  174 | `	  }` |
|      ! 0 |  175 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|        - |  176 | `    }` |
|        - |  177 | `    /* Find out what flags are present */` |
| 19190959 |  178 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
| 19190954 |  179 | `     flag_alternateform = flag_zeropad = 0;` |
|  9595477 |  180 | `    do{` |
| 19193101 |  181 | `      switch( c ){` |
|      ! 0 |  182 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      ! 0 |  183 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|      ! 0 |  184 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|       11 |  185 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     2134 |  186 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
| 19190954 |  187 | `        default:                                       break;` |
|        - |  188 | `      }` |
| 19193101 |  189 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|        - |  190 | `    /* Get the field width */` |
| 19190959 |  191 | `    width = 0;` |
| 19190959 |  192 | `    if( c=='*' ){` |
|      ! 0 |  193 | `      width = va_arg(ap,int);` |
|      ! 0 |  194 | `      if( width<0 ){` |
|      ! 0 |  195 | `        flag_leftjustify = 1;` |
|      ! 0 |  196 | `        width = -width;` |
|      ! 0 |  197 | `      }` |
|      ! 0 |  198 | `      c = *++zFormat;` |
|      ! 0 |  199 | `    }else{` |
| 19193155 |  200 | `      while( c>='0' && c<='9' ){` |
|     2198 |  201 | `        width = width*10 + c - '0';` |
|     2198 |  202 | `        c = *++zFormat;` |
|        2 |  203 | `      }` |
|        - |  204 | `    }` |
| 19190959 |  205 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|      ! 0 |  206 | `      width = SXFMT_BUFSIZ-10;` |
|      ! 0 |  207 | `    }` |
|        - |  208 | `    /* Get the precision */` |
| 19190959 |  209 | `	precision = -1;` |
| 19190959 |  210 | `    if( c=='.' ){` |
|  3775283 |  211 | `      precision = 0;` |
|  3775283 |  212 | `      c = *++zFormat;` |
|  3775283 |  213 | `      if( c=='*' ){` |
|  3775269 |  214 | `        precision = va_arg(ap,int);` |
|  3775269 |  215 | `        if( precision<0 ) precision = -precision;` |
|  3775269 |  216 | `        c = *++zFormat;` |
|  1887637 |  217 | `      }else{` |
|       30 |  218 | `        while( c>='0' && c<='9' ){` |
|       16 |  219 | `          precision = precision*10 + c - '0';` |
|       16 |  220 | `          c = *++zFormat;` |
|        2 |  221 | `        }` |
|        - |  222 | `      }` |
|  1887639 |  223 | `    }` |
|        - |  224 | `    /* Get the conversion type modifier */` |
| 19190959 |  225 | `	flag_long = 0;` |
| 19190959 |  226 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|    76463 |  227 | `      flag_long = (c == 'q') ? 2 : 1;` |
|    76463 |  228 | `      c = *++zFormat;` |
|    76463 |  229 | `	  if( c == 'l' ){` |
|        - |  230 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|      ! 0 |  231 | `		  flag_long = 2;` |
|      ! 0 |  232 | `	  }` |
|    38229 |  233 | `    }` |
|        - |  234 | `    /* Fetch the info entry for the field */` |
| 19190959 |  235 | `    infop = 0;` |
| 19190959 |  236 | `    xtype = SXFMT_ERROR;` |
| 84047961 |  237 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 84047961 |  238 | `      if( c==aFmt[idx].fmttype ){` |
| 19190959 |  239 | `        infop = &aFmt[idx];` |
| 19190959 |  240 | `		xtype = infop->type;` |
| 19190959 |  241 | `        break;` |
|        - |  242 | `      }` |
| 32428506 |  243 | `    }` |
|        - |  244 | `    /* zExtra is not used in this code path. */` |
|        - |  245 |  |
|        - |  246 | `    /*` |
|        - |  247 | `    ** At this point, variables are initialized as follows:` |
|        - |  248 | `    **` |
|        - |  249 | `    **   flag_alternateform          TRUE if a '#' is present.` |
|        - |  250 | `    **   flag_plussign               TRUE if a '+' is present.` |
|        - |  251 | `    **   flag_leftjustify            TRUE if a '-' is present or if the` |
|        - |  252 | `    **                               field width was negative.` |
|        - |  253 | `    **   flag_zeropad                TRUE if the width began with 0.` |
|        - |  254 | `    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed` |
|        - |  255 | `    **                               the conversion character.` |
|        - |  256 | `    **   flag_blanksign              TRUE if a ' ' is present.` |
|        - |  257 | `    **   width                       The specified field width.This is` |
|        - |  258 | `    **                               always non-negative.Zero is the default.` |
|        - |  259 | `    **   precision                   The specified precision.The default` |
|        - |  260 | `    **                               is -1.` |
|        - |  261 | `    **   xtype                       The class of the conversion.` |
|        - |  262 | `    **   infop                       Pointer to the appropriate info struct.` |
|        - |  263 | `    */` |
| 19190959 |  264 | `    switch( xtype ){` |
|    59719 |  265 | `      case SXFMT_RADIX:` |
|   119443 |  266 | `        if( flag_long > 0 ){` |
|    76463 |  267 | `			if( flag_long > 1 ){` |
|        - |  268 | `				/* BSD quad: expect a 64-bit integer */` |
|    76451 |  269 | `				longvalue = va_arg(ap,sxi64);` |
|    38228 |  270 | `			}else{` |
|       13 |  271 | `				longvalue = va_arg(ap,sxlong);` |
|        - |  272 | `			}` |
|    38234 |  273 | `		}else{` |
|    42985 |  274 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|    22681 |  275 | `				longvalue = va_arg(ap,sxi32);` |
|    11343 |  276 | `			}else{` |
|    20309 |  277 | `				longvalue = va_arg(ap,sxu32);` |
|        - |  278 | `			}` |
|        - |  279 | `		}` |
|        - |  280 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   119443 |  281 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|        - |  282 | `#if 1` |
|        - |  283 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|        - |  284 | `        ** I think this is stupid.*/` |
|   119443 |  285 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|        - |  286 | `#else` |
|        - |  287 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|        - |  288 | `        ** but leave the prefix for hex.*/` |
|        - |  289 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|        - |  290 | `#endif` |
|   119443 |  291 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|    99073 |  292 | `          if( longvalue<0 ){` |
|        - |  293 | `            /* Negate in unsigned space so INT64_MIN (where -longvalue would` |
|        - |  294 | `            ** overflow, UB that recent compilers exploit) yields the correct` |
|        - |  295 | `            ** magnitude 2^63 rather than garbage. */` |
|      475 |  296 | `            ulongvalue = (sxu64)0 - (sxu64)longvalue;` |
|      475 |  297 | `            prefix = '-';` |
|      240 |  298 | `          }else{` |
|    98603 |  299 | `            ulongvalue = (sxu64)longvalue;` |
|    98603 |  300 | `            if( flag_plussign )        prefix = '+';` |
|    98603 |  301 | `            else if( flag_blanksign )  prefix = ' ';` |
|    98603 |  302 | `            else                       prefix = 0;` |
|        - |  303 | `          }` |
|    49539 |  304 | `        }else{` |
|    20375 |  305 | `			ulongvalue = (sxu64)longvalue; /* print the full unsigned value as-is */` |
|    20375 |  306 | `			prefix = 0;` |
|        - |  307 | `		}` |
|   119443 |  308 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|     2134 |  309 | `          precision = width-(prefix!=0);` |
|     1066 |  310 | `        }` |
|   119443 |  311 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|        - |  312 | `        {` |
|        - |  313 | `          register char *cset;      /* Use registers for speed */` |
|        - |  314 | `          register int base;` |
|   119443 |  315 | `          cset = infop->charset;` |
|   119443 |  316 | `          base = infop->base;` |
|    59719 |  317 | `          do{                                           /* Convert to ascii */` |
|   288968 |  318 | `            *(--bufpt) = cset[ulongvalue%base];` |
|   288968 |  319 | `            ulongvalue = ulongvalue/base;` |
|   288968 |  320 | `          }while( ulongvalue>0 );` |
|        - |  321 | `        }` |
|   119443 |  322 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   121453 |  323 | `        for(idx=precision-length; idx>0; idx--){` |
|     2012 |  324 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|     1007 |  325 | `        }` |
|   119443 |  326 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|   119443 |  327 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|        - |  328 | `          char *pre, x;` |
|      ! 0 |  329 | `          pre = infop->prefix;` |
|      ! 0 |  330 | `          if( *bufpt!=pre[0] ){` |
|      ! 0 |  331 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|      ! 0 |  332 | `          }` |
|      ! 0 |  333 | `        }` |
|   119443 |  334 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   119443 |  335 | `        break;` |
|      ! 0 |  336 | `      case SXFMT_FLOAT:` |
|        - |  337 | `      case SXFMT_EXP:` |
|        - |  338 | `      case SXFMT_GENERIC:` |
|        - |  339 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      ! 0 |  340 | `		realvalue = va_arg(ap,double);` |
|        - |  341 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|      ! 0 |  342 | `        if( PH7_IS_NAN(realvalue) ){` |
|        - |  343 | `            /* lowercase nan consistent with libc */` |
|      ! 0 |  344 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|        - |  345 | `            /* the value has no sign; make sure prefix is clear */` |
|      ! 0 |  346 | `            prefix = 0;` |
|      ! 0 |  347 | `            bufpt = buf + 3;` |
|      ! 0 |  348 | `            goto float_done;` |
|        - |  349 | `        }` |
|      ! 0 |  350 | `        if( PH7_IS_INF(realvalue) ){` |
|      ! 0 |  351 | `            if( realvalue < 0.0 ){` |
|        - |  352 | `                /* negative infinity should be signed via prefix */` |
|      ! 0 |  353 | `                prefix = '-';` |
|      ! 0 |  354 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|      ! 0 |  355 | `                bufpt = buf + 3;` |
|      ! 0 |  356 | `            }else{` |
|        - |  357 | `                /* positive infinity treated like a plain value */` |
|      ! 0 |  358 | `                prefix = 0;` |
|      ! 0 |  359 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|      ! 0 |  360 | `                bufpt = buf + 3;` |
|        - |  361 | `            }` |
|      ! 0 |  362 | `            goto float_done;` |
|        - |  363 | `        }` |
|      ! 0 |  364 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|      ! 0 |  365 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|      ! 0 |  366 | `        if( realvalue<0.0 ){` |
|      ! 0 |  367 | `          realvalue = -realvalue;` |
|      ! 0 |  368 | `          prefix = '-';` |
|      ! 0 |  369 | `        }else{` |
|      ! 0 |  370 | `          if( flag_plussign )          prefix = '+';` |
|      ! 0 |  371 | `          else if( flag_blanksign )    prefix = ' ';` |
|      ! 0 |  372 | `          else                         prefix = 0;` |
|        - |  373 | `        }` |
|      ! 0 |  374 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|      ! 0 |  375 | `        rounder = 0.0;` |
|        - |  376 | `#if 0` |
|        - |  377 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|        - |  378 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|        - |  379 | `#else` |
|        - |  380 | `        /* It makes more sense to use 0.5 */` |
|      ! 0 |  381 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|        - |  382 | `#endif` |
|      ! 0 |  383 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|        - |  384 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|      ! 0 |  385 | `        exp = 0;` |
|      ! 0 |  386 | `        if( realvalue>0.0 ){` |
|      ! 0 |  387 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|      ! 0 |  388 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|      ! 0 |  389 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|      ! 0 |  390 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|      ! 0 |  391 | `          if( exp>350 \|\| exp<-350 ){` |
|      ! 0 |  392 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|      ! 0 |  393 | `            bufpt = buf + 3;` |
|      ! 0 |  394 | `            goto float_done;` |
|        - |  395 | `          }` |
|      ! 0 |  396 | `        }` |
|      ! 0 |  397 | `        bufpt = buf;` |
|        - |  398 | `        /*` |
|        - |  399 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|        - |  400 | `        ** or etFLOAT, as appropriate.` |
|        - |  401 | `        */` |
|      ! 0 |  402 | `        flag_exp = xtype==SXFMT_EXP;` |
|      ! 0 |  403 | `        if( xtype!=SXFMT_FLOAT ){` |
|      ! 0 |  404 | `          realvalue += rounder;` |
|      ! 0 |  405 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|      ! 0 |  406 | `        }` |
|      ! 0 |  407 | `        if( xtype==SXFMT_GENERIC ){` |
|      ! 0 |  408 | `          flag_rtz = !flag_alternateform;` |
|      ! 0 |  409 | `          if( exp<-4 \|\| exp>precision ){` |
|      ! 0 |  410 | `            xtype = SXFMT_EXP;` |
|      ! 0 |  411 | `          }else{` |
|      ! 0 |  412 | `            precision = precision - exp;` |
|      ! 0 |  413 | `            xtype = SXFMT_FLOAT;` |
|        - |  414 | `          }` |
|      ! 0 |  415 | `        }else{` |
|      ! 0 |  416 | `          flag_rtz = 0;` |
|        - |  417 | `        }` |
|        - |  418 | `        /*` |
|        - |  419 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|        - |  420 | `        ** the precision is too large to fit in buf[].` |
|        - |  421 | `        */` |
|      ! 0 |  422 | `        nsd = 0;` |
|      ! 0 |  423 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|      ! 0 |  424 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|      ! 0 |  425 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|      ! 0 |  426 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|      ! 0 |  427 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|      ! 0 |  428 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|      ! 0 |  429 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|      ! 0 |  430 | `            *(bufpt++) = '0';` |
|      ! 0 |  431 | `          }` |
|      ! 0 |  432 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|      ! 0 |  433 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|      ! 0 |  434 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|      ! 0 |  435 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|      ! 0 |  436 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|      ! 0 |  437 | `          }` |
|      ! 0 |  438 | `          bufpt++;                            /* point to next free slot */` |
|      ! 0 |  439 | `        }else{    /* etEXP or etGENERIC */` |
|      ! 0 |  440 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|      ! 0 |  441 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|      ! 0 |  442 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|      ! 0 |  443 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|      ! 0 |  444 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|      ! 0 |  445 | `          bufpt--;                            /* point to last digit */` |
|      ! 0 |  446 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|      ! 0 |  447 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|      ! 0 |  448 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|      ! 0 |  449 | `          }` |
|      ! 0 |  450 | `          bufpt++;                            /* point to next free slot */` |
|      ! 0 |  451 | `          if( exp \|\| flag_exp ){` |
|      ! 0 |  452 | `            *(bufpt++) = infop->charset[0];` |
|      ! 0 |  453 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|      ! 0 |  454 | `            else       { *(bufpt++) = '+'; }` |
|      ! 0 |  455 | `            if( exp>=100 ){` |
|      ! 0 |  456 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|      ! 0 |  457 | `              exp %= 100;` |
|      ! 0 |  458 | `            }` |
|      ! 0 |  459 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|      ! 0 |  460 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|      ! 0 |  461 | `          }` |
|        - |  462 | `        }` |
|      ! 0 |  463 | `        float_done:` |
|        - |  464 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|        - |  465 | `        ** Note that the number is in the usual order, not reversed as with` |
|        - |  466 | `        ** integer conversions.*/` |
|      ! 0 |  467 | `        length = (int)(bufpt-buf);` |
|      ! 0 |  468 | `        bufpt = buf;` |
|        - |  469 |  |
|        - |  470 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|        - |  471 | `        ** set and we are not left justified */` |
|      ! 0 |  472 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|        - |  473 | `          int i;` |
|      ! 0 |  474 | `          int nPad = width - length;` |
|      ! 0 |  475 | `          for(i=width; i>=nPad; i--){` |
|      ! 0 |  476 | `            bufpt[i] = bufpt[i-nPad];` |
|      ! 0 |  477 | `          }` |
|      ! 0 |  478 | `          i = prefix!=0;` |
|      ! 0 |  479 | `          while( nPad-- ) bufpt[i++] = '0';` |
|      ! 0 |  480 | `          length = width;` |
|      ! 0 |  481 | `        }` |
|        - |  482 | `#else` |
|        - |  483 | `         bufpt = " ";` |
|        - |  484 | `		 length = (int)sizeof(" ") - 1;` |
|        - |  485 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|      ! 0 |  486 | `        break;` |
|      ! 0 |  487 | `      case SXFMT_SIZE:{` |
|      ! 0 |  488 | `		 int *pSize = va_arg(ap,int *);` |
|      ! 0 |  489 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|      ! 0 |  490 | `		 length = width = 0;` |
|        - |  491 | `					  }` |
|      ! 0 |  492 | `        break;` |
|      ! 0 |  493 | `      case SXFMT_PERCENT:` |
|      ! 0 |  494 | `        buf[0] = '%';` |
|      ! 0 |  495 | `        bufpt = buf;` |
|      ! 0 |  496 | `        length = 1;` |
|      ! 0 |  497 | `        break;` |
|     5140 |  498 | `      case SXFMT_CHARX:` |
|    10285 |  499 | `        c = va_arg(ap,int);` |
|    10285 |  500 | `		buf[0] = (char)c;` |
|        - |  501 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    10285 |  502 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|    10285 |  503 | `        if( precision>=0 ){` |
|      ! 0 |  504 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|      ! 0 |  505 | `          length = precision;` |
|      ! 0 |  506 | `        }else{` |
|    10285 |  507 | `          length =1;` |
|        - |  508 | `        }` |
|    10285 |  509 | `        bufpt = buf;` |
|    10285 |  510 | `        break;` |
|  3829017 |  511 | `      case SXFMT_STRING:` |
|  7658039 |  512 | `        bufpt = va_arg(ap,char*);` |
|  7658039 |  513 | `        if( bufpt==0 ){` |
|        - |  514 | ``		  /* An explicit precision of 0 (`%.*s` with a 0 length arg) is an EMPTY`` |
|        - |  515 | `		   * string, not a missing one: emit nothing. SyBlobData() returns NULL for` |
|        - |  516 | `		   * a zero-length blob, so the legacy "print a single space for a NULL` |
|        - |  517 | `		   * pointer" fallback rendered an empty array key / blob as " " (visible in` |
|        - |  518 | ``		   * var_dump/print_r `["" ]` and the `Undefined array key ""` warning). A`` |
|        - |  519 | ``		   * genuine NULL string (`%s`/`%z`, precision < 0) still prints the space. */`` |
|       37 |  520 | `		  if( precision == 0 ){ length = 0; break; }` |
|      ! 0 |  521 | `          bufpt = " ";` |
|      ! 0 |  522 | `		  length = (int)sizeof(" ")-1;` |
|      ! 0 |  523 | `		  break;` |
|        - |  524 | `        }` |
|  7658005 |  525 | `		length = precision;` |
|  7658005 |  526 | `		if( precision < 0 ){` |
|        - |  527 | `			/* Symisc extension */` |
|  3882761 |  528 | `			length = (int)SyStrlen(bufpt);` |
|  1941378 |  529 | `		}` |
|  7658005 |  530 | `        if( precision>=0 && precision<length ) length = precision;` |
|  7658005 |  531 | `        break;` |
|  5701601 |  532 | `	case SXFMT_RAWSTR:{` |
|        - |  533 | `		/* Symisc extension */` |
| 11403207 |  534 | `		SyString *pStr = va_arg(ap,SyString *);` |
| 11403207 |  535 | `		if( pStr == 0 ){` |
|      ! 0 |  536 | `			 bufpt = " ";` |
|      ! 0 |  537 | `		     length = (int)sizeof(char);` |
|      ! 0 |  538 | `		     break;` |
|        - |  539 | `		}` |
| 11403207 |  540 | `		if( pStr->zString == 0 ){` |
|        - |  541 | `			 /* A SyString with a NULL buffer is the EMPTY string (SyBlobData()` |
|        - |  542 | `			  * returns NULL for a zero-length blob): emit nothing, not the legacy` |
|        - |  543 | `			  * single-space fallback that rendered an empty key as " ". */` |
|        7 |  544 | `			 length = 0;` |
|        7 |  545 | `		     break;` |
|        - |  546 | `		}` |
| 11403201 |  547 | `		bufpt = (char *)pStr->zString;` |
| 11403201 |  548 | `		length = (int)pStr->nByte;` |
| 11403201 |  549 | `		break;` |
|        - |  550 | `					  }` |
|      ! 0 |  551 | `      case SXFMT_ERROR:` |
|      ! 0 |  552 | `        buf[0] = '?';` |
|      ! 0 |  553 | `        bufpt = buf;` |
|      ! 0 |  554 | `		length = (int)sizeof(char);` |
|      ! 0 |  555 | `        if( c==0 ) zFormat--;` |
|      ! 0 |  556 | `        break;` |
|        - |  557 | `    }/* End switch over the format type */` |
|        - |  558 | `    /*` |
|        - |  559 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|        - |  560 | `    ** "length" characters long.The field width is "width".Do` |
|        - |  561 | `    ** the output.` |
|        - |  562 | `    */` |
| 19190959 |  563 | `    if( !flag_leftjustify ){` |
|        - |  564 | `      register int nspace;` |
| 19190959 |  565 | `      nspace = width-length;` |
| 19190959 |  566 | `      if( nspace>0 ){` |
|       31 |  567 | `        while( nspace>=etSPACESIZE ){` |
|      ! 0 |  568 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|      ! 0 |  569 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  570 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  571 | `			}` |
|      ! 0 |  572 | `			nspace -= etSPACESIZE;` |
|      ! 0 |  573 | `        }` |
|       31 |  574 | `        if( nspace>0 ){` |
|       31 |  575 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|       31 |  576 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  577 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  578 | `			}` |
|       15 |  579 | `		}` |
|       15 |  580 | `      }` |
|  9595477 |  581 | `    }` |
| 19190959 |  582 | `    if( length>0 ){` |
| 19190173 |  583 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
| 19190173 |  584 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  585 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  586 | `		}` |
|  9595084 |  587 | `    }` |
| 19190959 |  588 | `    if( flag_leftjustify ){` |
|        - |  589 | `      register int nspace;` |
|      ! 0 |  590 | `      nspace = width-length;` |
|      ! 0 |  591 | `      if( nspace>0 ){` |
|      ! 0 |  592 | `        while( nspace>=etSPACESIZE ){` |
|      ! 0 |  593 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|      ! 0 |  594 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  595 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  596 | `			}` |
|      ! 0 |  597 | `			nspace -= etSPACESIZE;` |
|      ! 0 |  598 | `        }` |
|      ! 0 |  599 | `        if( nspace>0 ){` |
|      ! 0 |  600 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|      ! 0 |  601 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  602 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|        - |  603 | `			}` |
|      ! 0 |  604 | `		}` |
|      ! 0 |  605 | `      }` |
|      ! 0 |  606 | `    }` |
|  9595482 |  607 | `  }/* End for loop over the format string */` |
|  3711909 |  608 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  3881375 |  609 | `}` |
|        - |  610 | `/* SPDX-SnippetEnd */` |
| 38792384 |  611 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|        5 |  612 | `{` |
| 38792389 |  613 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 38792389 |  614 | `	sxi32 rc = SXERR_ABORT;` |
| 38792389 |  615 | `	switch(pConsumer->nType){` |
|       75 |  616 | `	case SXFMT_CONS_PROC:` |
|        - |  617 | `			/* User callback */` |
|      151 |  618 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|      151 |  619 | `			break;` |
| 19396117 |  620 | `	case SXFMT_CONS_BLOB:` |
|        - |  621 | `			/* Blob consumer */` |
| 38792239 |  622 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 38792234 |  623 | `			break;` |
|      ! 0 |  624 | `		default:` |
|        - |  625 | `			/* Unknown consumer */` |
|      ! 0 |  626 | `			break;` |
|        - |  627 | `	}` |
|        - |  628 | `	/* Update total number of bytes consumed so far */` |
| 38792389 |  629 | `	pConsumer->nLen += nLen;` |
| 38792389 |  630 | `	pConsumer->rc = rc;` |
| 38792389 |  631 | `	return rc;` |
|        5 |  632 | `}` |
|  7762740 |  633 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|        5 |  634 | `{` |
|        - |  635 | `	SyFmtConsumer sCons;` |
|  7762745 |  636 | `	sCons.nType = nType;` |
|  7762745 |  637 | `	sCons.rc = SXRET_OK;` |
|  7762745 |  638 | `	sCons.nLen = 0;` |
|  7762745 |  639 | `	if( pOutLen ){` |
|   483709 |  640 | `		*pOutLen = 0;` |
|   241852 |  641 | `	}` |
|  7762745 |  642 | `	switch(nType){` |
|        5 |  643 | `	case SXFMT_CONS_PROC:` |
|        - |  644 | `#if defined(UNTRUST)` |
|        - |  645 | `			if( xUserCons == 0 ){` |
|        - |  646 | `				return SXERR_EMPTY;` |
|        - |  647 | `			}` |
|        - |  648 | `#endif` |
|       11 |  649 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|       11 |  650 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|       11 |  651 | `		break;` |
|  3881365 |  652 | `		case SXFMT_CONS_BLOB:` |
|  7762735 |  653 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  7762735 |  654 | `			break;` |
|      ! 0 |  655 | `		default:` |
|      ! 0 |  656 | `			return SXERR_UNKNOWN;` |
|        - |  657 | `	}` |
|  7762745 |  658 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  7762745 |  659 | `	if( pOutLen ){` |
|   483709 |  660 | `		*pOutLen = sCons.nLen;` |
|   241852 |  661 | `	}` |
|  7762745 |  662 | `	return sCons.rc;` |
|  3881375 |  663 | `}` |
|       10 |  664 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|        1 |  665 | `{` |
|        - |  666 | `	va_list ap;` |
|        - |  667 | `	sxi32 rc;` |
|        - |  668 | `#if defined(UNTRUST)` |
|        - |  669 | `	if( SX_EMPTY_STR(zFormat) ){` |
|        - |  670 | `		return SXERR_EMPTY;` |
|        - |  671 | `	}` |
|        - |  672 | `#endif` |
|       11 |  673 | `	va_start(ap,zFormat);` |
|       11 |  674 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|       11 |  675 | `	va_end(ap);` |
|       11 |  676 | `	return rc;` |
|        1 |  677 | `}` |
|   448996 |  678 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|        5 |  679 | `{` |
|        - |  680 | `	va_list ap;` |
|        - |  681 | `	sxu32 n;` |
|        - |  682 | `#if defined(UNTRUST)` |
|        - |  683 | `	if( SX_EMPTY_STR(zFormat) ){` |
|        - |  684 | `		return 0;` |
|        - |  685 | `	}` |
|        - |  686 | `#endif` |
|   449001 |  687 | `	va_start(ap,zFormat);` |
|   449001 |  688 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|   449001 |  689 | `	va_end(ap);` |
|   449001 |  690 | `	return n;` |
|        5 |  691 | `}` |
|    34708 |  692 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|        5 |  693 | `{` |
|    34713 |  694 | `	sxu32 n = 0; /* cc warning */` |
|        - |  695 | `#if defined(UNTRUST)` |
|        - |  696 | `	if( SX_EMPTY_STR(zFormat) ){` |
|        - |  697 | `		return 0;` |
|        - |  698 | `	}` |
|        - |  699 | `#endif` |
|    34713 |  700 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    34713 |  701 | `	return n;` |
|        5 |  702 | `}` |
|  7279026 |  703 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|        5 |  704 | `{` |
|        - |  705 | `	SyBlob sBlob;` |
|        - |  706 | `	va_list ap;` |
|        - |  707 | `	sxu32 n;` |
|        - |  708 | `#if defined(UNTRUST)` |
|        - |  709 | `	if( SX_EMPTY_STR(zFormat) ){` |
|        - |  710 | `		return 0;` |
|        - |  711 | `	}` |
|        - |  712 | `#endif` |
|  7279031 |  713 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|      ! 0 |  714 | `		return 0;` |
|        - |  715 | `	}` |
|  7279031 |  716 | `	va_start(ap,zFormat);` |
|  7279031 |  717 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  7279031 |  718 | `	va_end(ap);` |
|  7279031 |  719 | `	n = SyBlobLength(&sBlob);` |
|        - |  720 | `	/* Append the null terminator */` |
|  7279031 |  721 | `	sBlob.mByte++;` |
|  7279031 |  722 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  7279031 |  723 | `	return n;` |
|  3639518 |  724 | `}` |
|        - |  725 |  |
