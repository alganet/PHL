# src/sx/sxfmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 291/413 lines (70.46%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "sxtypes.h"` |
|      - |    7 | `#include "sxmacros.h"` |
|      - |    8 | `#include "sxset.h"` |
|      - |    9 | `#include "sxmem.h"` |
|      - |   10 | `#include "sxfmt.h"` |
|      - |   11 | `#include "sxstr.h"` |
|      - |   12 |  |
|      - |   13 | `#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - |   14 | `/*` |
|      - |   15 | `** Conversion types fall into various categories as defined by the` |
|      - |   16 | `** following enumeration.` |
|      - |   17 | `*/` |
|      - |   18 | `#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - |   19 | `#define SXFMT_FLOAT       2 /* Floating point.%f */` |
|      - |   20 | `#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - |   21 | `#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - |   22 | `#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - |   23 | `#define SXFMT_STRING      6 /* Strings.%s */` |
|      - |   24 | `#define SXFMT_PERCENT     7 /* Percent symbol.%% */` |
|      - |   25 | `#define SXFMT_CHARX       8 /* Characters.%c */` |
|      - |   26 | `#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - |   27 | `/* Extension by Symisc Systems */` |
|      - |   28 | `#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */` |
|      - |   29 | `#define SXFMT_UNUSED     15` |
|      - |   30 | `/*` |
|      - |   31 | `** Allowed values for SyFmtInfo.flags` |
|      - |   32 | `*/` |
|      - |   33 | `#define SXFLAG_SIGNED	0x01` |
|      - |   34 | `#define SXFLAG_UNSIGNED 0x02` |
|      - |   35 | `/* Allowed values for SyFmtConsumer.nType */` |
|      - |   36 | `#define SXFMT_CONS_PROC		1	/* Consumer is a procedure */` |
|      - |   37 | `#define SXFMT_CONS_STR		2	/* Consumer is a managed string */` |
|      - |   38 | `#define SXFMT_CONS_FILE		5	/* Consumer is an open File */` |
|      - |   39 | `#define SXFMT_CONS_BLOB		6	/* Consumer is a BLOB */` |
|      - |   40 | `/*` |
|      - |   41 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - |   42 | `** by an instance of the following structure` |
|      - |   43 | `*/` |
|      - |   44 | `typedef struct SyFmtInfo SyFmtInfo;` |
|      - |   45 | `struct SyFmtInfo` |
|      - |   46 |  |
|      - |   47 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - |   48 | `  sxu8 base;     /* The base for radix conversion */` |
|      - |   49 | `  int flags;    /* One or more of SXFLAG_ constants below */` |
|      - |   50 | `  sxu8 type;     /* Conversion paradigm */` |
|      - |   51 | `  char *charset; /* The character set for conversion */` |
|      - |   52 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - |   53 | `};` |
|      - |   54 | `typedef struct SyFmtConsumer SyFmtConsumer;` |
|      - |   55 | `struct SyFmtConsumer` |
|      - |   56 |  |
|      - |   57 | `	sxu32 nLen; /* Total output length */` |
|      - |   58 | `	sxi32 nType; /* Type of the consumer see below */` |
|      - |   59 | `	sxi32 rc;	/* Consumer return value;Abort processing if rc != SXRET_OK */` |
|      - |   60 | ` union{` |
|      - |   61 | `	struct{` |
|      - |   62 | `	ProcConsumer xUserConsumer;` |
|      - |   63 | `	void *pUserData;` |
|      - |   64 | `	}sFunc;` |
|      - |   65 | `	SyBlob *pBlob;` |
|      - |   66 | ` }uConsumer;` |
|      - |   67 | `};` |
|      - |   68 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|   1532 |   69 | `static int getdigit(sxlongreal *val,int *cnt)` |
|      2 |   70 |  |
|      - |   71 | `  sxlongreal d;` |
|      - |   72 | `  int digit;` |
|      - |   73 |  |
|   1534 |   74 | `  if( (*cnt)++ >= 16 ){` |
|     31 |   75 | `	  return '0';` |
|      - |   76 | `  }` |
|   1504 |   77 | `  digit = (int)*val;` |
|   1504 |   78 | `  d = digit;` |
|   1504 |   79 | `   *val = (*val - d)*10.0;` |
|   1504 |   80 | `  return digit + '0' ;` |
|    768 |   81 |  |
|      - |   82 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|      - |   83 | `/*` |
|      - |   84 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|      - |   85 | ` * extended by Symisc Systems to fit its need.` |
|      - |   86 | ` * Status: Public Domain` |
|      - |   87 | ` */` |
|  98036 |   88 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
|      2 |   89 |  |
|      - |   90 | `	/*` |
|      - |   91 | `	 * The following table is searched linearly, so it is good to put the most frequently` |
|      - |   92 | `	 * used conversion types first.` |
|      - |   93 | `	 */` |
|      - |   94 | `static const SyFmtInfo aFmt[] = {` |
|      - |   95 | `  {  'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789",0    },` |
|      - |   96 | `  {  's',  0, 0, SXFMT_STRING,     0,                  0    },` |
|      - |   97 | `  {  'c',  0, 0, SXFMT_CHARX,      0,                  0    },` |
|      - |   98 | `  {  'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - |   99 | `  {  'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - |  100 | `         /* -- Extensions by Symisc Systems -- */` |
|      - |  101 | `  {  'z',  0, 0, SXFMT_RAWSTR,     0,                   0   }, /* Pointer to a raw string (SyString *) */` |
|      - |  102 | `  {  'B',  2, 0, SXFMT_RADIX,      "01",                "b0"},` |
|      - |  103 | `         /* -- End of Extensions -- */` |
|      - |  104 | `  {  'o',  8, 0, SXFMT_RADIX,      "01234567",         "0"  },` |
|      - |  105 | `  {  'u', 10, 0, SXFMT_RADIX,      "0123456789",       0    },` |
|      - |  106 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      - |  107 | `  {  'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0    },` |
|      - |  108 | `  {  'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0    },` |
|      - |  109 | `  {  'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0    },` |
|      - |  110 | `  {  'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0    },` |
|      - |  111 | `  {  'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0    },` |
|      - |  112 | `#endif` |
|      - |  113 | `  {  'i', 10, SXFLAG_SIGNED, SXFMT_RADIX,"0123456789", 0    },` |
|      - |  114 | `  {  'n',  0, 0, SXFMT_SIZE,       0,                  0    },` |
|      - |  115 | `  {  '%',  0, 0, SXFMT_PERCENT,    0,                  0    },` |
|      - |  116 | `  {  'p', 10, 0, SXFMT_RADIX,      "0123456789",       0    }` |
|      - |  117 | `};` |
|      - |  118 | `  int c;                     /* Next character in the format string */` |
|      - |  119 | `  char *bufpt;               /* Pointer to the conversion buffer */` |
|      - |  120 | `  int precision;             /* Precision of the current field */` |
|      - |  121 | `  int length;                /* Length of the field */` |
|      - |  122 | `  int idx;                   /* A general purpose loop counter */` |
|      - |  123 | `  int width;                 /* Width of the current field */` |
|      - |  124 | `  sxu8 flag_leftjustify;   /* True if "-" flag is present */` |
|      - |  125 | `  sxu8 flag_plussign;      /* True if "+" flag is present */` |
|      - |  126 | `  sxu8 flag_blanksign;     /* True if " " flag is present */` |
|      - |  127 | `  sxu8 flag_alternateform; /* True if "#" flag is present */` |
|      - |  128 | `  sxu8 flag_zeropad;       /* True if field width constant starts with zero */` |
|      - |  129 | `  sxu8 flag_long;          /* True if "l" flag is present */` |
|      - |  130 | `  sxi64 longvalue;         /* Value for integer types */` |
|      - |  131 | `  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */` |
|      - |  132 | `  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */` |
|      - |  133 | `  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/` |
|  98038 |  134 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
|      - |  135 | `  sxu8 xtype;              /* Conversion paradigm */` |
|      - |  136 | `  static char spaces[] = "                                                  ";` |
|      - |  137 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|      - |  138 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|      - |  139 | `  sxlongreal realvalue;    /* Value for real types */` |
|      - |  140 | `  int  exp;                /* exponent of real numbers */` |
|      - |  141 | `  double rounder;          /* Used for rounding floating point values */` |
|      - |  142 | `  sxu8 flag_dp;            /* True if decimal point should be shown */` |
|      - |  143 | `  sxu8 flag_rtz;           /* True if trailing zeros should be removed */` |
|      - |  144 | `  sxu8 flag_exp;           /* True to force display of the exponent */` |
|      - |  145 | `  int nsd;                 /* Number of significant digits returned */` |
|      - |  146 | `#endif` |
|      - |  147 | `  int rc;` |
|      - |  148 |  |
|  98038 |  149 | `  length = 0;` |
|  98038 |  150 | `  bufpt = 0;` |
| 280474 |  151 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 219500 |  152 | `    if( c!='%' ){` |
|      - |  153 | `      unsigned int amt;` |
| 144830 |  154 | `      bufpt = (char *)zFormat;` |
| 144830 |  155 | `      amt = 1;` |
| 241244 |  156 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
| 144830 |  157 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
| 144830 |  158 | `	  if( rc != SXRET_OK ){` |
|    ! 0 |  159 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  160 | `	  }` |
| 144830 |  161 | `      if( c==0 ){` |
|  37064 |  162 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|      - |  163 | `	  }` |
|  53883 |  164 | `    }` |
| 182438 |  165 | `    if( (c=(*++zFormat))==0 ){` |
|    ! 0 |  166 | `      errorflag = 1;` |
|    ! 0 |  167 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|    ! 0 |  168 | `	  if( rc != SXRET_OK ){` |
|    ! 0 |  169 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  170 | `	  }` |
|    ! 0 |  171 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|      - |  172 | `    }` |
|      - |  173 | `    /* Find out what flags are present */` |
| 182438 |  174 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
| 182436 |  175 | `     flag_alternateform = flag_zeropad = 0;` |
|  91218 |  176 | `    do{` |
| 182542 |  177 | `      switch( c ){` |
|    ! 0 |  178 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 |  179 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 |  180 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     16 |  181 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     89 |  182 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
| 182436 |  183 | `        default:                                       break;` |
|      - |  184 | `      }` |
| 182542 |  185 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|      - |  186 | `    /* Get the field width */` |
| 182438 |  187 | `    width = 0;` |
| 182438 |  188 | `    if( c=='*' ){` |
|    ! 0 |  189 | `      width = va_arg(ap,int);` |
|    ! 0 |  190 | `      if( width<0 ){` |
|    ! 0 |  191 | `        flag_leftjustify = 1;` |
|    ! 0 |  192 | `        width = -width;` |
|    ! 0 |  193 | `      }` |
|    ! 0 |  194 | `      c = *++zFormat;` |
|    ! 0 |  195 | `    }else{` |
| 182580 |  196 | `      while( c>='0' && c<='9' ){` |
|    144 |  197 | `        width = width*10 + c - '0';` |
|    144 |  198 | `        c = *++zFormat;` |
|      2 |  199 | `      }` |
|      - |  200 | `    }` |
| 182438 |  201 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|    ! 0 |  202 | `      width = SXFMT_BUFSIZ-10;` |
|    ! 0 |  203 | `    }` |
|      - |  204 | `    /* Get the precision */` |
| 182438 |  205 | `	precision = -1;` |
| 182438 |  206 | `    if( c=='.' ){` |
|  35692 |  207 | `      precision = 0;` |
|  35692 |  208 | `      c = *++zFormat;` |
|  35692 |  209 | `      if( c=='*' ){` |
|  35594 |  210 | `        precision = va_arg(ap,int);` |
|  35594 |  211 | `        if( precision<0 ) precision = -precision;` |
|  35594 |  212 | `        c = *++zFormat;` |
|  17798 |  213 | `      }else{` |
|    296 |  214 | `        while( c>='0' && c<='9' ){` |
|    198 |  215 | `          precision = precision*10 + c - '0';` |
|    198 |  216 | `          c = *++zFormat;` |
|      2 |  217 | `        }` |
|      - |  218 | `      }` |
|  17845 |  219 | `    }` |
|      - |  220 | `    /* Get the conversion type modifier */` |
| 182438 |  221 | `	flag_long = 0;` |
| 182438 |  222 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|  54246 |  223 | `      flag_long = (c == 'q') ? 2 : 1;` |
|  54246 |  224 | `      c = *++zFormat;` |
|  54246 |  225 | `	  if( c == 'l' ){` |
|      - |  226 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|    ! 0 |  227 | `		  flag_long = 2;` |
|    ! 0 |  228 | `	  }` |
|  27122 |  229 | `    }` |
|      - |  230 | `    /* Fetch the info entry for the field */` |
| 182438 |  231 | `    infop = 0;` |
| 182438 |  232 | `    xtype = SXFMT_ERROR;` |
| 660740 |  233 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 660740 |  234 | `      if( c==aFmt[idx].fmttype ){` |
| 182438 |  235 | `        infop = &aFmt[idx];` |
| 182438 |  236 | `		xtype = infop->type;` |
| 182438 |  237 | `        break;` |
|      - |  238 | `      }` |
| 239153 |  239 | `    }` |
|      - |  240 | `    /* zExtra is not used in this code path. */` |
|      - |  241 |  |
|      - |  242 | `    /*` |
|      - |  243 | `    ** At this point, variables are initialized as follows:` |
|      - |  244 | `    **` |
|      - |  245 | `    **   flag_alternateform          TRUE if a '#' is present.` |
|      - |  246 | `    **   flag_plussign               TRUE if a '+' is present.` |
|      - |  247 | `    **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - |  248 | `    **                               field width was negative.` |
|      - |  249 | `    **   flag_zeropad                TRUE if the width began with 0.` |
|      - |  250 | `    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed` |
|      - |  251 | `    **                               the conversion character.` |
|      - |  252 | `    **   flag_blanksign              TRUE if a ' ' is present.` |
|      - |  253 | `    **   width                       The specified field width.This is` |
|      - |  254 | `    **                               always non-negative.Zero is the default.` |
|      - |  255 | `    **   precision                   The specified precision.The default` |
|      - |  256 | `    **                               is -1.` |
|      - |  257 | `    **   xtype                       The class of the conversion.` |
|      - |  258 | `    **   infop                       Pointer to the appropriate info struct.` |
|      - |  259 | `    */` |
| 182438 |  260 | `    switch( xtype ){` |
|  27580 |  261 | `      case SXFMT_RADIX:` |
|  55162 |  262 | `        if( flag_long > 0 ){` |
|  54246 |  263 | `			if( flag_long > 1 ){` |
|      - |  264 | `				/* BSD quad: expect a 64-bit integer */` |
|  54238 |  265 | `				longvalue = va_arg(ap,sxi64);` |
|  27120 |  266 | `			}else{` |
|      9 |  267 | `				longvalue = va_arg(ap,sxlong);` |
|      - |  268 | `			}` |
|  27124 |  269 | `		}else{` |
|    918 |  270 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|    236 |  271 | `				longvalue = va_arg(ap,sxi32);` |
|    119 |  272 | `			}else{` |
|    684 |  273 | `				longvalue = va_arg(ap,sxu32);` |
|      - |  274 | `			}` |
|      - |  275 | `		}` |
|      - |  276 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|  55162 |  277 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|      - |  278 | `#if 1` |
|      - |  279 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - |  280 | `        ** I think this is stupid.*/` |
|  55162 |  281 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|      - |  282 | `#else` |
|      - |  283 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - |  284 | `        ** but leave the prefix for hex.*/` |
|      - |  285 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|      - |  286 | `#endif` |
|  55162 |  287 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|  54468 |  288 | `          if( longvalue<0 ){` |
|     47 |  289 | `            longvalue = -longvalue;` |
|      - |  290 | `			/* Ticket 1433-003 */` |
|     47 |  291 | `			if( longvalue < 0 ){` |
|      - |  292 | `				/* Overflow */` |
|    ! 0 |  293 | `				longvalue= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 |  294 | `			}` |
|     47 |  295 | `            prefix = '-';` |
|  54445 |  296 | `          }else if( flag_plussign )  prefix = '+';` |
|  54420 |  297 | `          else if( flag_blanksign )  prefix = ' ';` |
|  54420 |  298 | `          else                       prefix = 0;` |
|  27235 |  299 | `        }else{` |
|    696 |  300 | `			if( longvalue<0 ){` |
|    ! 0 |  301 | `				longvalue = -longvalue;` |
|      - |  302 | `				/* Ticket 1433-003 */` |
|    ! 0 |  303 | `				if( longvalue < 0 ){` |
|      - |  304 | `					/* Overflow */` |
|    ! 0 |  305 | `					longvalue= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 |  306 | `				}` |
|    ! 0 |  307 | `			}` |
|    696 |  308 | `			prefix = 0;` |
|      - |  309 | `		}` |
|  55162 |  310 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|     89 |  311 | `          precision = width-(prefix!=0);` |
|     44 |  312 | `        }` |
|  55162 |  313 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|      - |  314 | `        {` |
|      - |  315 | `          register char *cset;      /* Use registers for speed */` |
|      - |  316 | `          register int base;` |
|  55162 |  317 | `          cset = infop->charset;` |
|  55162 |  318 | `          base = infop->base;` |
|  27580 |  319 | `          do{                                           /* Convert to ascii */` |
| 176587 |  320 | `            *(--bufpt) = cset[longvalue%base];` |
| 176587 |  321 | `            longvalue = longvalue/base;` |
| 176587 |  322 | `          }while( longvalue>0 );` |
|      - |  323 | `        }` |
|  55162 |  324 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|  55205 |  325 | `        for(idx=precision-length; idx>0; idx--){` |
|     44 |  326 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|     20 |  327 | `        }` |
|  55162 |  328 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|  55162 |  329 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|      - |  330 | `          char *pre, x;` |
|      5 |  331 | `          pre = infop->prefix;` |
|      5 |  332 | `          if( *bufpt!=pre[0] ){` |
|     13 |  333 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|      2 |  334 | `          }` |
|      2 |  335 | `        }` |
|  55162 |  336 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|  55162 |  337 | `        break;` |
|     50 |  338 | `      case SXFMT_FLOAT:` |
|      - |  339 | `      case SXFMT_EXP:` |
|      - |  340 | `      case SXFMT_GENERIC:` |
|      - |  341 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|    102 |  342 | `		realvalue = va_arg(ap,double);` |
|      - |  343 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|    102 |  344 | `        if( PH7_IS_NAN(realvalue) ){` |
|      - |  345 | `            /* lowercase nan consistent with libc */` |
|    ! 0 |  346 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|      - |  347 | `            /* the value has no sign; make sure prefix is clear */` |
|    ! 0 |  348 | `            prefix = 0;` |
|    ! 0 |  349 | `            bufpt = buf + 3;` |
|    ! 0 |  350 | `            goto float_done;` |
|      - |  351 | `        }` |
|    102 |  352 | `        if( PH7_IS_INF(realvalue) ){` |
|    ! 0 |  353 | `            if( realvalue < 0.0 ){` |
|      - |  354 | `                /* negative infinity should be signed via prefix */` |
|    ! 0 |  355 | `                prefix = '-';` |
|    ! 0 |  356 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|    ! 0 |  357 | `                bufpt = buf + 3;` |
|    ! 0 |  358 | `            }else{` |
|      - |  359 | `                /* positive infinity treated like a plain value */` |
|    ! 0 |  360 | `                prefix = 0;` |
|    ! 0 |  361 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|    ! 0 |  362 | `                bufpt = buf + 3;` |
|      - |  363 | `            }` |
|    ! 0 |  364 | `            goto float_done;` |
|      - |  365 | `        }` |
|    102 |  366 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|    102 |  367 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|    102 |  368 | `        if( realvalue<0.0 ){` |
|     11 |  369 | `          realvalue = -realvalue;` |
|     11 |  370 | `          prefix = '-';` |
|      6 |  371 | `        }else{` |
|     92 |  372 | `          if( flag_plussign )          prefix = '+';` |
|     92 |  373 | `          else if( flag_blanksign )    prefix = ' ';` |
|     92 |  374 | `          else                         prefix = 0;` |
|      - |  375 | `        }` |
|    102 |  376 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|    102 |  377 | `        rounder = 0.0;` |
|      - |  378 | `#if 0` |
|      - |  379 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - |  380 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - |  381 | `#else` |
|      - |  382 | `        /* It makes more sense to use 0.5 */` |
|   1534 |  383 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - |  384 | `#endif` |
|    102 |  385 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|      - |  386 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|    102 |  387 | `        exp = 0;` |
|    102 |  388 | `        if( realvalue>0.0 ){` |
|     99 |  389 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|    141 |  390 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     95 |  391 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|    119 |  392 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     95 |  393 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 |  394 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|    ! 0 |  395 | `            bufpt = buf + 3;` |
|    ! 0 |  396 | `            goto float_done;` |
|      - |  397 | `          }` |
|     47 |  398 | `        }` |
|    102 |  399 | `        bufpt = buf;` |
|      - |  400 | `        /*` |
|      - |  401 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - |  402 | `        ** or etFLOAT, as appropriate.` |
|      - |  403 | `        */` |
|    102 |  404 | `        flag_exp = xtype==SXFMT_EXP;` |
|    102 |  405 | `        if( xtype!=SXFMT_FLOAT ){` |
|    100 |  406 | `          realvalue += rounder;` |
|    100 |  407 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|     49 |  408 | `        }` |
|    102 |  409 | `        if( xtype==SXFMT_GENERIC ){` |
|    100 |  410 | `          flag_rtz = !flag_alternateform;` |
|    100 |  411 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 |  412 | `            xtype = SXFMT_EXP;` |
|    ! 0 |  413 | `          }else{` |
|    100 |  414 | `            precision = precision - exp;` |
|    100 |  415 | `            xtype = SXFMT_FLOAT;` |
|      - |  416 | `          }` |
|     51 |  417 | `        }else{` |
|      3 |  418 | `          flag_rtz = 0;` |
|      - |  419 | `        }` |
|      - |  420 | `        /*` |
|      - |  421 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - |  422 | `        ** the precision is too large to fit in buf[].` |
|      - |  423 | `        */` |
|    102 |  424 | `        nsd = 0;` |
|    152 |  425 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|    102 |  426 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    102 |  427 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|    102 |  428 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|    256 |  429 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|    102 |  430 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|    114 |  431 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     13 |  432 | `            *(bufpt++) = '0';` |
|      7 |  433 | `          }` |
|   1468 |  434 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|    102 |  435 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|    102 |  436 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|   1132 |  437 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|    100 |  438 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     49 |  439 | `          }` |
|    102 |  440 | `          bufpt++;                            /* point to next free slot */` |
|     52 |  441 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 |  442 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 |  443 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|    ! 0 |  444 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 |  445 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|    ! 0 |  446 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|    ! 0 |  447 | `          bufpt--;                            /* point to last digit */` |
|    ! 0 |  448 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 |  449 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|    ! 0 |  450 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|    ! 0 |  451 | `          }` |
|    ! 0 |  452 | `          bufpt++;                            /* point to next free slot */` |
|    ! 0 |  453 | `          if( exp \|\| flag_exp ){` |
|    ! 0 |  454 | `            *(bufpt++) = infop->charset[0];` |
|    ! 0 |  455 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 |  456 | `            else       { *(bufpt++) = '+'; }` |
|    ! 0 |  457 | `            if( exp>=100 ){` |
|    ! 0 |  458 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 |  459 | `              exp %= 100;` |
|    ! 0 |  460 | `            }` |
|    ! 0 |  461 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 |  462 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 |  463 | `          }` |
|      - |  464 | `        }` |
|    ! 0 |  465 | `        float_done:` |
|      - |  466 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - |  467 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - |  468 | `        ** integer conversions.*/` |
|    102 |  469 | `        length = (int)(bufpt-buf);` |
|    102 |  470 | `        bufpt = buf;` |
|      - |  471 |  |
|      - |  472 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - |  473 | `        ** set and we are not left justified */` |
|    102 |  474 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - |  475 | `          int i;` |
|    ! 0 |  476 | `          int nPad = width - length;` |
|    ! 0 |  477 | `          for(i=width; i>=nPad; i--){` |
|    ! 0 |  478 | `            bufpt[i] = bufpt[i-nPad];` |
|    ! 0 |  479 | `          }` |
|    ! 0 |  480 | `          i = prefix!=0;` |
|    ! 0 |  481 | `          while( nPad-- ) bufpt[i++] = '0';` |
|    ! 0 |  482 | `          length = width;` |
|    ! 0 |  483 | `        }` |
|      - |  484 | `#else` |
|      - |  485 | `         bufpt = " ";` |
|      - |  486 | `		 length = (int)sizeof(" ") - 1;` |
|      - |  487 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|    102 |  488 | `        break;` |
|    ! 0 |  489 | `      case SXFMT_SIZE:{` |
|    ! 0 |  490 | `		 int *pSize = va_arg(ap,int *);` |
|    ! 0 |  491 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|    ! 0 |  492 | `		 length = width = 0;` |
|      - |  493 | `					  }` |
|    ! 0 |  494 | `        break;` |
|      1 |  495 | `      case SXFMT_PERCENT:` |
|      3 |  496 | `        buf[0] = '%';` |
|      3 |  497 | `        bufpt = buf;` |
|      3 |  498 | `        length = 1;` |
|      3 |  499 | `        break;` |
|   3280 |  500 | `      case SXFMT_CHARX:` |
|   6562 |  501 | `        c = va_arg(ap,int);` |
|   6562 |  502 | `		buf[0] = (char)c;` |
|      - |  503 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   6562 |  504 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|   6562 |  505 | `        if( precision>=0 ){` |
|    ! 0 |  506 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|    ! 0 |  507 | `          length = precision;` |
|    ! 0 |  508 | `        }else{` |
|   6562 |  509 | `          length =1;` |
|      - |  510 | `        }` |
|   6562 |  511 | `        bufpt = buf;` |
|   6562 |  512 | `        break;` |
|  18066 |  513 | `      case SXFMT_STRING:` |
|  36134 |  514 | `        bufpt = va_arg(ap,char*);` |
|  36134 |  515 | `        if( bufpt==0 ){` |
|    ! 0 |  516 | `          bufpt = " ";` |
|    ! 0 |  517 | `		  length = (int)sizeof(" ")-1;` |
|    ! 0 |  518 | `		  break;` |
|      - |  519 | `        }` |
|  36134 |  520 | `		length = precision;` |
|  36134 |  521 | `		if( precision < 0 ){` |
|      - |  522 | `			/* Symisc extension */` |
|    544 |  523 | `			length = (int)SyStrlen(bufpt);` |
|    271 |  524 | `		}` |
|  36134 |  525 | `        if( precision>=0 && precision<length ) length = precision;` |
|  36134 |  526 | `        break;` |
|  42241 |  527 | `	case SXFMT_RAWSTR:{` |
|      - |  528 | `		/* Symisc extension */` |
|  84484 |  529 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  84484 |  530 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|    ! 0 |  531 | `			 bufpt = " ";` |
|    ! 0 |  532 | `		     length = (int)sizeof(char);` |
|    ! 0 |  533 | `		     break;` |
|      - |  534 | `		}` |
|  84484 |  535 | `		bufpt = (char *)pStr->zString;` |
|  84484 |  536 | `		length = (int)pStr->nByte;` |
|  84484 |  537 | `		break;` |
|      - |  538 | `					  }` |
|    ! 0 |  539 | `      case SXFMT_ERROR:` |
|    ! 0 |  540 | `        buf[0] = '?';` |
|    ! 0 |  541 | `        bufpt = buf;` |
|    ! 0 |  542 | `		length = (int)sizeof(char);` |
|    ! 0 |  543 | `        if( c==0 ) zFormat--;` |
|    ! 0 |  544 | `        break;` |
|      - |  545 | `    }/* End switch over the format type */` |
|      - |  546 | `    /*` |
|      - |  547 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|      - |  548 | `    ** "length" characters long.The field width is "width".Do` |
|      - |  549 | `    ** the output.` |
|      - |  550 | `    */` |
| 182438 |  551 | `    if( !flag_leftjustify ){` |
|      - |  552 | `      register int nspace;` |
| 182438 |  553 | `      nspace = width-length;` |
| 182438 |  554 | `      if( nspace>0 ){` |
|     31 |  555 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  556 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  557 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  558 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  559 | `			}` |
|    ! 0 |  560 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  561 | `        }` |
|     31 |  562 | `        if( nspace>0 ){` |
|     31 |  563 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|     31 |  564 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  565 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  566 | `			}` |
|     15 |  567 | `		}` |
|     15 |  568 | `      }` |
|  91218 |  569 | `    }` |
| 182438 |  570 | `    if( length>0 ){` |
| 182438 |  571 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
| 182438 |  572 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  573 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  574 | `		}` |
|  91218 |  575 | `    }` |
| 182438 |  576 | `    if( flag_leftjustify ){` |
|      - |  577 | `      register int nspace;` |
|    ! 0 |  578 | `      nspace = width-length;` |
|    ! 0 |  579 | `      if( nspace>0 ){` |
|    ! 0 |  580 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  581 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  582 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  583 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  584 | `			}` |
|    ! 0 |  585 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  586 | `        }` |
|    ! 0 |  587 | `        if( nspace>0 ){` |
|    ! 0 |  588 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|    ! 0 |  589 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  590 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  591 | `			}` |
|    ! 0 |  592 | `		}` |
|    ! 0 |  593 | `      }` |
|    ! 0 |  594 | `    }` |
|  91220 |  595 | `  }/* End for loop over the format string */` |
|  60976 |  596 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  49020 |  597 |  |
| 327294 |  598 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|      2 |  599 |  |
| 327296 |  600 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 327296 |  601 | `	sxi32 rc = SXERR_ABORT;` |
| 327296 |  602 | `	switch(pConsumer->nType){` |
|     65 |  603 | `	case SXFMT_CONS_PROC:` |
|      - |  604 | `			/* User callback */` |
|    131 |  605 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|    131 |  606 | `			break;` |
| 163582 |  607 | `	case SXFMT_CONS_BLOB:` |
|      - |  608 | `			/* Blob consumer */` |
| 327166 |  609 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 327164 |  610 | `			break;` |
|    ! 0 |  611 | `		default:` |
|      - |  612 | `			/* Unknown consumer */` |
|    ! 0 |  613 | `			break;` |
|      - |  614 | `	}` |
|      - |  615 | `	/* Update total number of bytes consumed so far */` |
| 327296 |  616 | `	pConsumer->nLen += nLen;` |
| 327296 |  617 | `	pConsumer->rc = rc;` |
| 327296 |  618 | `	return rc;` |
|      2 |  619 |  |
|  98036 |  620 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|      2 |  621 |  |
|      - |  622 | `	SyFmtConsumer sCons;` |
|  98038 |  623 | `	sCons.nType = nType;` |
|  98038 |  624 | `	sCons.rc = SXRET_OK;` |
|  98038 |  625 | `	sCons.nLen = 0;` |
|  98038 |  626 | `	if( pOutLen ){` |
|  62390 |  627 | `		*pOutLen = 0;` |
|  31194 |  628 | `	}` |
|  98038 |  629 | `	switch(nType){` |
|      5 |  630 | `	case SXFMT_CONS_PROC:` |
|      - |  631 | `#if defined(UNTRUST)` |
|      - |  632 | `			if( xUserCons == 0 ){` |
|      - |  633 | `				return SXERR_EMPTY;` |
|      - |  634 | `			}` |
|      - |  635 | `#endif` |
|     11 |  636 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|     11 |  637 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|     11 |  638 | `		break;` |
|  49013 |  639 | `		case SXFMT_CONS_BLOB:` |
|  98028 |  640 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  98028 |  641 | `			break;` |
|    ! 0 |  642 | `		default:` |
|    ! 0 |  643 | `			return SXERR_UNKNOWN;` |
|      - |  644 | `	}` |
|  98038 |  645 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  98038 |  646 | `	if( pOutLen ){` |
|  62390 |  647 | `		*pOutLen = sCons.nLen;` |
|  31194 |  648 | `	}` |
|  98038 |  649 | `	return sCons.rc;` |
|  49020 |  650 |  |
|     10 |  651 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|      1 |  652 |  |
|      - |  653 | `	va_list ap;` |
|      - |  654 | `	sxi32 rc;` |
|      - |  655 | `#if defined(UNTRUST)` |
|      - |  656 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  657 | `		return SXERR_EMPTY;` |
|      - |  658 | `	}` |
|      - |  659 | `#endif` |
|     11 |  660 | `	va_start(ap,zFormat);` |
|     11 |  661 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|     11 |  662 | `	va_end(ap);` |
|     11 |  663 | `	return rc;` |
|      1 |  664 |  |
|  61660 |  665 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|      2 |  666 |  |
|      - |  667 | `	va_list ap;` |
|      - |  668 | `	sxu32 n;` |
|      - |  669 | `#if defined(UNTRUST)` |
|      - |  670 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  671 | `		return 0;` |
|      - |  672 | `	}` |
|      - |  673 | `#endif` |
|  61662 |  674 | `	va_start(ap,zFormat);` |
|  61662 |  675 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|  61662 |  676 | `	va_end(ap);` |
|  61662 |  677 | `	return n;` |
|      2 |  678 |  |
|    728 |  679 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|      2 |  680 |  |
|    730 |  681 | `	sxu32 n = 0; /* cc warning */` |
|      - |  682 | `#if defined(UNTRUST)` |
|      - |  683 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  684 | `		return 0;` |
|      - |  685 | `	}` |
|      - |  686 | `#endif` |
|    730 |  687 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    730 |  688 | `	return n;` |
|      2 |  689 |  |
|  35638 |  690 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|      2 |  691 |  |
|      - |  692 | `	SyBlob sBlob;` |
|      - |  693 | `	va_list ap;` |
|      - |  694 | `	sxu32 n;` |
|      - |  695 | `#if defined(UNTRUST)` |
|      - |  696 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  697 | `		return 0;` |
|      - |  698 | `	}` |
|      - |  699 | `#endif` |
|  35640 |  700 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|    ! 0 |  701 | `		return 0;` |
|      - |  702 | `	}` |
|  35640 |  703 | `	va_start(ap,zFormat);` |
|  35640 |  704 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  35640 |  705 | `	va_end(ap);` |
|  35640 |  706 | `	n = SyBlobLength(&sBlob);` |
|      - |  707 | `	/* Append the null terminator */` |
|  35640 |  708 | `	sBlob.mByte++;` |
|  35640 |  709 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  35640 |  710 | `	return n;` |
|  17821 |  711 |  |
|      - |  712 |  |
