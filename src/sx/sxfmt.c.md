# src/sx/sxfmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 289/397 lines (72.80%)

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
|   1472 |   69 | `static int getdigit(sxlongreal *val,int *cnt)` |
|      2 |   70 |  |
|      - |   71 | `  sxlongreal d;` |
|      - |   72 | `  int digit;` |
|      - |   73 |  |
|   1474 |   74 | `  if( (*cnt)++ >= 16 ){` |
|     31 |   75 | `	  return '0';` |
|      - |   76 | `  }` |
|   1444 |   77 | `  digit = (int)*val;` |
|   1444 |   78 | `  d = digit;` |
|   1444 |   79 | `   *val = (*val - d)*10.0;` |
|   1444 |   80 | `  return digit + '0' ;` |
|    738 |   81 |  |
|      - |   82 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|      - |   83 | `/*` |
|      - |   84 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|      - |   85 | ` * extended by Symisc Systems to fit its need.` |
|      - |   86 | ` * Status: Public Domain` |
|      - |   87 | ` */` |
|  97048 |   88 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
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
|  97050 |  134 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
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
|  97050 |  149 | `  length = 0;` |
|  97050 |  150 | `  bufpt = 0;` |
| 276666 |  151 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 215798 |  152 | `    if( c!='%' ){` |
|      - |  153 | `      unsigned int amt;` |
| 141330 |  154 | `      bufpt = (char *)zFormat;` |
| 141330 |  155 | `      amt = 1;` |
| 235438 |  156 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
| 141330 |  157 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
| 141330 |  158 | `	  if( rc != SXRET_OK ){` |
|    ! 0 |  159 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  160 | `	  }` |
| 141330 |  161 | `      if( c==0 ){` |
|  36182 |  162 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|      - |  163 | `	  }` |
|  52574 |  164 | `    }` |
| 179618 |  165 | `    if( (c=(*++zFormat))==0 ){` |
|    ! 0 |  166 | `      errorflag = 1;` |
|    ! 0 |  167 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|    ! 0 |  168 | `	  if( rc != SXRET_OK ){` |
|    ! 0 |  169 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  170 | `	  }` |
|    ! 0 |  171 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|      - |  172 | `    }` |
|      - |  173 | `    /* Find out what flags are present */` |
| 179618 |  174 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
| 179616 |  175 | `     flag_alternateform = flag_zeropad = 0;` |
|  89808 |  176 | `    do{` |
| 179722 |  177 | `      switch( c ){` |
|    ! 0 |  178 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      3 |  179 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 |  180 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     16 |  181 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     89 |  182 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
| 179616 |  183 | `        default:                                       break;` |
|      - |  184 | `      }` |
| 179722 |  185 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|      - |  186 | `    /* Get the field width */` |
| 179618 |  187 | `    width = 0;` |
| 179618 |  188 | `    if( c=='*' ){` |
|    ! 0 |  189 | `      width = va_arg(ap,int);` |
|    ! 0 |  190 | `      if( width<0 ){` |
|    ! 0 |  191 | `        flag_leftjustify = 1;` |
|    ! 0 |  192 | `        width = -width;` |
|    ! 0 |  193 | `      }` |
|    ! 0 |  194 | `      c = *++zFormat;` |
|    ! 0 |  195 | `    }else{` |
| 179760 |  196 | `      while( c>='0' && c<='9' ){` |
|    144 |  197 | `        width = width*10 + c - '0';` |
|    144 |  198 | `        c = *++zFormat;` |
|      2 |  199 | `      }` |
|      - |  200 | `    }` |
| 179618 |  201 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|    ! 0 |  202 | `      width = SXFMT_BUFSIZ-10;` |
|    ! 0 |  203 | `    }` |
|      - |  204 | `    /* Get the precision */` |
| 179618 |  205 | `	precision = -1;` |
| 179618 |  206 | `    if( c=='.' ){` |
|  34818 |  207 | `      precision = 0;` |
|  34818 |  208 | `      c = *++zFormat;` |
|  34818 |  209 | `      if( c=='*' ){` |
|  34724 |  210 | `        precision = va_arg(ap,int);` |
|  34724 |  211 | `        if( precision<0 ) precision = -precision;` |
|  34724 |  212 | `        c = *++zFormat;` |
|  17363 |  213 | `      }else{` |
|    284 |  214 | `        while( c>='0' && c<='9' ){` |
|    190 |  215 | `          precision = precision*10 + c - '0';` |
|    190 |  216 | `          c = *++zFormat;` |
|      2 |  217 | `        }` |
|      - |  218 | `      }` |
|  17408 |  219 | `    }` |
|      - |  220 | `    /* Get the conversion type modifier */` |
| 179618 |  221 | `	flag_long = 0;` |
| 179618 |  222 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|  54192 |  223 | `      flag_long = (c == 'q') ? 2 : 1;` |
|  54192 |  224 | `      c = *++zFormat;` |
|  54192 |  225 | `	  if( c == 'l' ){` |
|      - |  226 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|    ! 0 |  227 | `		  flag_long = 2;` |
|    ! 0 |  228 | `	  }` |
|  27095 |  229 | `    }` |
|      - |  230 | `    /* Fetch the info entry for the field */` |
| 179618 |  231 | `    infop = 0;` |
| 179618 |  232 | `    xtype = SXFMT_ERROR;` |
| 647742 |  233 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 647742 |  234 | `      if( c==aFmt[idx].fmttype ){` |
| 179618 |  235 | `        infop = &aFmt[idx];` |
| 179618 |  236 | `		xtype = infop->type;` |
| 179618 |  237 | `        break;` |
|      - |  238 | `      }` |
| 234064 |  239 | `    }` |
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
| 179618 |  260 | `    switch( xtype ){` |
|  27549 |  261 | `      case SXFMT_RADIX:` |
|  55100 |  262 | `        if( flag_long > 0 ){` |
|  54192 |  263 | `			if( flag_long > 1 ){` |
|      - |  264 | `				/* BSD quad: expect a 64-bit integer */` |
|  54184 |  265 | `				longvalue = va_arg(ap,sxi64);` |
|  27093 |  266 | `			}else{` |
|      9 |  267 | `				longvalue = va_arg(ap,sxlong);` |
|      - |  268 | `			}` |
|  27097 |  269 | `		}else{` |
|    910 |  270 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|    228 |  271 | `				longvalue = va_arg(ap,sxi32);` |
|    115 |  272 | `			}else{` |
|    684 |  273 | `				longvalue = va_arg(ap,sxu32);` |
|      - |  274 | `			}` |
|      - |  275 | `		}` |
|      - |  276 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|  55100 |  277 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|      - |  278 | `#if 1` |
|      - |  279 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - |  280 | `        ** I think this is stupid.*/` |
|  55100 |  281 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|      - |  282 | `#else` |
|      - |  283 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - |  284 | `        ** but leave the prefix for hex.*/` |
|      - |  285 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|      - |  286 | `#endif` |
|  55100 |  287 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|  54406 |  288 | `          if( longvalue<0 ){` |
|     47 |  289 | `            longvalue = -longvalue;` |
|      - |  290 | `			/* Ticket 1433-003 */` |
|     47 |  291 | `			if( longvalue < 0 ){` |
|      - |  292 | `				/* Overflow */` |
|    ! 0 |  293 | `				longvalue= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 |  294 | `			}` |
|     47 |  295 | `            prefix = '-';` |
|  54383 |  296 | `          }else if( flag_plussign )  prefix = '+';` |
|  54358 |  297 | `          else if( flag_blanksign )  prefix = ' ';` |
|  54358 |  298 | `          else                       prefix = 0;` |
|  27204 |  299 | `        }else{` |
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
|  55100 |  310 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|     89 |  311 | `          precision = width-(prefix!=0);` |
|     44 |  312 | `        }` |
|  55100 |  313 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|      - |  314 | `        {` |
|      - |  315 | `          register char *cset;      /* Use registers for speed */` |
|      - |  316 | `          register int base;` |
|  55100 |  317 | `          cset = infop->charset;` |
|  55100 |  318 | `          base = infop->base;` |
|  27549 |  319 | `          do{                                           /* Convert to ascii */` |
| 176403 |  320 | `            *(--bufpt) = cset[longvalue%base];` |
| 176403 |  321 | `            longvalue = longvalue/base;` |
| 176403 |  322 | `          }while( longvalue>0 );` |
|      - |  323 | `        }` |
|  55100 |  324 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|  55150 |  325 | `        for(idx=precision-length; idx>0; idx--){` |
|     51 |  326 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|     26 |  327 | `        }` |
|  55100 |  328 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|  55100 |  329 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|      - |  330 | `          char *pre, x;` |
|      5 |  331 | `          pre = infop->prefix;` |
|      5 |  332 | `          if( *bufpt!=pre[0] ){` |
|     13 |  333 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|      2 |  334 | `          }` |
|      2 |  335 | `        }` |
|  55100 |  336 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|  55100 |  337 | `        break;` |
|     48 |  338 | `      case SXFMT_FLOAT:` |
|      - |  339 | `      case SXFMT_EXP:` |
|      - |  340 | `      case SXFMT_GENERIC:` |
|      - |  341 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|     98 |  342 | `		realvalue = va_arg(ap,double);` |
|     98 |  343 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     98 |  344 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|     98 |  345 | `        if( realvalue<0.0 ){` |
|     11 |  346 | `          realvalue = -realvalue;` |
|     11 |  347 | `          prefix = '-';` |
|      6 |  348 | `        }else{` |
|     88 |  349 | `          if( flag_plussign )          prefix = '+';` |
|     88 |  350 | `          else if( flag_blanksign )    prefix = ' ';` |
|     88 |  351 | `          else                         prefix = 0;` |
|      - |  352 | `        }` |
|     98 |  353 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|     98 |  354 | `        rounder = 0.0;` |
|      - |  355 | `#if 0` |
|      - |  356 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|      - |  357 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|      - |  358 | `#else` |
|      - |  359 | `        /* It makes more sense to use 0.5 */` |
|   1474 |  360 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|      - |  361 | `#endif` |
|     98 |  362 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|      - |  363 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     98 |  364 | `        exp = 0;` |
|     98 |  365 | `        if( realvalue>0.0 ){` |
|     93 |  366 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|    135 |  367 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     89 |  368 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|    113 |  369 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     89 |  370 | `          if( exp>350 \|\| exp<-350 ){` |
|    ! 0 |  371 | `            bufpt = "NaN";` |
|    ! 0 |  372 | `            length = 3;` |
|    ! 0 |  373 | `            break;` |
|      - |  374 | `          }` |
|     44 |  375 | `        }` |
|     98 |  376 | `        bufpt = buf;` |
|      - |  377 | `        /*` |
|      - |  378 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|      - |  379 | `        ** or etFLOAT, as appropriate.` |
|      - |  380 | `        */` |
|     98 |  381 | `        flag_exp = xtype==SXFMT_EXP;` |
|     98 |  382 | `        if( xtype!=SXFMT_FLOAT ){` |
|     96 |  383 | `          realvalue += rounder;` |
|     96 |  384 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|     47 |  385 | `        }` |
|     98 |  386 | `        if( xtype==SXFMT_GENERIC ){` |
|     96 |  387 | `          flag_rtz = !flag_alternateform;` |
|     96 |  388 | `          if( exp<-4 \|\| exp>precision ){` |
|    ! 0 |  389 | `            xtype = SXFMT_EXP;` |
|    ! 0 |  390 | `          }else{` |
|     96 |  391 | `            precision = precision - exp;` |
|     96 |  392 | `            xtype = SXFMT_FLOAT;` |
|      - |  393 | `          }` |
|     49 |  394 | `        }else{` |
|      3 |  395 | `          flag_rtz = 0;` |
|      - |  396 | `        }` |
|      - |  397 | `        /*` |
|      - |  398 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|      - |  399 | `        ** the precision is too large to fit in buf[].` |
|      - |  400 | `        */` |
|     98 |  401 | `        nsd = 0;` |
|     98 |  402 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|     98 |  403 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     98 |  404 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|     98 |  405 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|    248 |  406 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     98 |  407 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|    110 |  408 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|     13 |  409 | `            *(bufpt++) = '0';` |
|      7 |  410 | `          }` |
|   1408 |  411 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     98 |  412 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|     98 |  413 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|   1082 |  414 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     96 |  415 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     47 |  416 | `          }` |
|     98 |  417 | `          bufpt++;                            /* point to next free slot */` |
|     50 |  418 | `        }else{    /* etEXP or etGENERIC */` |
|    ! 0 |  419 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|    ! 0 |  420 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|    ! 0 |  421 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|    ! 0 |  422 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|    ! 0 |  423 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|    ! 0 |  424 | `          bufpt--;                            /* point to last digit */` |
|    ! 0 |  425 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|    ! 0 |  426 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|    ! 0 |  427 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|    ! 0 |  428 | `          }` |
|    ! 0 |  429 | `          bufpt++;                            /* point to next free slot */` |
|    ! 0 |  430 | `          if( exp \|\| flag_exp ){` |
|    ! 0 |  431 | `            *(bufpt++) = infop->charset[0];` |
|    ! 0 |  432 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|    ! 0 |  433 | `            else       { *(bufpt++) = '+'; }` |
|    ! 0 |  434 | `            if( exp>=100 ){` |
|    ! 0 |  435 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|    ! 0 |  436 | `              exp %= 100;` |
|    ! 0 |  437 | `            }` |
|    ! 0 |  438 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|    ! 0 |  439 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|    ! 0 |  440 | `          }` |
|      - |  441 | `        }` |
|      - |  442 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|      - |  443 | `        ** Note that the number is in the usual order, not reversed as with` |
|      - |  444 | `        ** integer conversions.*/` |
|     98 |  445 | `        length = (int)(bufpt-buf);` |
|     98 |  446 | `        bufpt = buf;` |
|      - |  447 |  |
|      - |  448 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - |  449 | `        ** set and we are not left justified */` |
|     98 |  450 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - |  451 | `          int i;` |
|    ! 0 |  452 | `          int nPad = width - length;` |
|    ! 0 |  453 | `          for(i=width; i>=nPad; i--){` |
|    ! 0 |  454 | `            bufpt[i] = bufpt[i-nPad];` |
|    ! 0 |  455 | `          }` |
|    ! 0 |  456 | `          i = prefix!=0;` |
|    ! 0 |  457 | `          while( nPad-- ) bufpt[i++] = '0';` |
|    ! 0 |  458 | `          length = width;` |
|    ! 0 |  459 | `        }` |
|      - |  460 | `#else` |
|      - |  461 | `         bufpt = " ";` |
|      - |  462 | `		 length = (int)sizeof(" ") - 1;` |
|      - |  463 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|     98 |  464 | `        break;` |
|    ! 0 |  465 | `      case SXFMT_SIZE:{` |
|    ! 0 |  466 | `		 int *pSize = va_arg(ap,int *);` |
|    ! 0 |  467 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|    ! 0 |  468 | `		 length = width = 0;` |
|      - |  469 | `					  }` |
|    ! 0 |  470 | `        break;` |
|      1 |  471 | `      case SXFMT_PERCENT:` |
|      3 |  472 | `        buf[0] = '%';` |
|      3 |  473 | `        bufpt = buf;` |
|      3 |  474 | `        length = 1;` |
|      3 |  475 | `        break;` |
|   3256 |  476 | `      case SXFMT_CHARX:` |
|   6514 |  477 | `        c = va_arg(ap,int);` |
|   6514 |  478 | `		buf[0] = (char)c;` |
|      - |  479 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   6514 |  480 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|   6514 |  481 | `        if( precision>=0 ){` |
|    ! 0 |  482 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|    ! 0 |  483 | `          length = precision;` |
|    ! 0 |  484 | `        }else{` |
|   6514 |  485 | `          length =1;` |
|      - |  486 | `        }` |
|   6514 |  487 | `        bufpt = buf;` |
|   6514 |  488 | `        break;` |
|  17629 |  489 | `      case SXFMT_STRING:` |
|  35260 |  490 | `        bufpt = va_arg(ap,char*);` |
|  35260 |  491 | `        if( bufpt==0 ){` |
|    ! 0 |  492 | `          bufpt = " ";` |
|    ! 0 |  493 | `		  length = (int)sizeof(" ")-1;` |
|    ! 0 |  494 | `		  break;` |
|      - |  495 | `        }` |
|  35260 |  496 | `		length = precision;` |
|  35260 |  497 | `		if( precision < 0 ){` |
|      - |  498 | `			/* Symisc extension */` |
|    540 |  499 | `			length = (int)SyStrlen(bufpt);` |
|    269 |  500 | `		}` |
|  35260 |  501 | `        if( precision>=0 && precision<length ) length = precision;` |
|  35260 |  502 | `        break;` |
|  41325 |  503 | `	case SXFMT_RAWSTR:{` |
|      - |  504 | `		/* Symisc extension */` |
|  82652 |  505 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  82652 |  506 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|    ! 0 |  507 | `			 bufpt = " ";` |
|    ! 0 |  508 | `		     length = (int)sizeof(char);` |
|    ! 0 |  509 | `		     break;` |
|      - |  510 | `		}` |
|  82652 |  511 | `		bufpt = (char *)pStr->zString;` |
|  82652 |  512 | `		length = (int)pStr->nByte;` |
|  82652 |  513 | `		break;` |
|      - |  514 | `					  }` |
|    ! 0 |  515 | `      case SXFMT_ERROR:` |
|    ! 0 |  516 | `        buf[0] = '?';` |
|    ! 0 |  517 | `        bufpt = buf;` |
|    ! 0 |  518 | `		length = (int)sizeof(char);` |
|    ! 0 |  519 | `        if( c==0 ) zFormat--;` |
|    ! 0 |  520 | `        break;` |
|      - |  521 | `    }/* End switch over the format type */` |
|      - |  522 | `    /*` |
|      - |  523 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|      - |  524 | `    ** "length" characters long.The field width is "width".Do` |
|      - |  525 | `    ** the output.` |
|      - |  526 | `    */` |
| 179618 |  527 | `    if( !flag_leftjustify ){` |
|      - |  528 | `      register int nspace;` |
| 179618 |  529 | `      nspace = width-length;` |
| 179618 |  530 | `      if( nspace>0 ){` |
|     31 |  531 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  532 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  533 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  534 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  535 | `			}` |
|    ! 0 |  536 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  537 | `        }` |
|     31 |  538 | `        if( nspace>0 ){` |
|     31 |  539 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|     31 |  540 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  541 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  542 | `			}` |
|     15 |  543 | `		}` |
|     15 |  544 | `      }` |
|  89808 |  545 | `    }` |
| 179618 |  546 | `    if( length>0 ){` |
| 179618 |  547 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
| 179618 |  548 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  549 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  550 | `		}` |
|  89808 |  551 | `    }` |
| 179618 |  552 | `    if( flag_leftjustify ){` |
|      - |  553 | `      register int nspace;` |
|    ! 0 |  554 | `      nspace = width-length;` |
|    ! 0 |  555 | `      if( nspace>0 ){` |
|    ! 0 |  556 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 |  557 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|    ! 0 |  558 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  559 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  560 | `			}` |
|    ! 0 |  561 | `			nspace -= etSPACESIZE;` |
|    ! 0 |  562 | `        }` |
|    ! 0 |  563 | `        if( nspace>0 ){` |
|    ! 0 |  564 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|    ! 0 |  565 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  566 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - |  567 | `			}` |
|    ! 0 |  568 | `		}` |
|    ! 0 |  569 | `      }` |
|    ! 0 |  570 | `    }` |
|  89810 |  571 | `  }/* End for loop over the format string */` |
|  60870 |  572 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  48526 |  573 |  |
| 320974 |  574 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|      2 |  575 |  |
| 320976 |  576 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 320976 |  577 | `	sxi32 rc = SXERR_ABORT;` |
| 320976 |  578 | `	switch(pConsumer->nType){` |
|     65 |  579 | `	case SXFMT_CONS_PROC:` |
|      - |  580 | `			/* User callback */` |
|    131 |  581 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|    131 |  582 | `			break;` |
| 160422 |  583 | `	case SXFMT_CONS_BLOB:` |
|      - |  584 | `			/* Blob consumer */` |
| 320846 |  585 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 320844 |  586 | `			break;` |
|    ! 0 |  587 | `		default:` |
|      - |  588 | `			/* Unknown consumer */` |
|    ! 0 |  589 | `			break;` |
|      - |  590 | `	}` |
|      - |  591 | `	/* Update total number of bytes consumed so far */` |
| 320976 |  592 | `	pConsumer->nLen += nLen;` |
| 320976 |  593 | `	pConsumer->rc = rc;` |
| 320976 |  594 | `	return rc;` |
|      2 |  595 |  |
|  97048 |  596 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|      2 |  597 |  |
|      - |  598 | `	SyFmtConsumer sCons;` |
|  97050 |  599 | `	sCons.nType = nType;` |
|  97050 |  600 | `	sCons.rc = SXRET_OK;` |
|  97050 |  601 | `	sCons.nLen = 0;` |
|  97050 |  602 | `	if( pOutLen ){` |
|  62272 |  603 | `		*pOutLen = 0;` |
|  31135 |  604 | `	}` |
|  97050 |  605 | `	switch(nType){` |
|      5 |  606 | `	case SXFMT_CONS_PROC:` |
|      - |  607 | `#if defined(UNTRUST)` |
|      - |  608 | `			if( xUserCons == 0 ){` |
|      - |  609 | `				return SXERR_EMPTY;` |
|      - |  610 | `			}` |
|      - |  611 | `#endif` |
|     11 |  612 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|     11 |  613 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|     11 |  614 | `		break;` |
|  48519 |  615 | `		case SXFMT_CONS_BLOB:` |
|  97040 |  616 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  97040 |  617 | `			break;` |
|    ! 0 |  618 | `		default:` |
|    ! 0 |  619 | `			return SXERR_UNKNOWN;` |
|      - |  620 | `	}` |
|  97050 |  621 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  97050 |  622 | `	if( pOutLen ){` |
|  62272 |  623 | `		*pOutLen = sCons.nLen;` |
|  31135 |  624 | `	}` |
|  97050 |  625 | `	return sCons.rc;` |
|  48526 |  626 |  |
|     10 |  627 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|      1 |  628 |  |
|      - |  629 | `	va_list ap;` |
|      - |  630 | `	sxi32 rc;` |
|      - |  631 | `#if defined(UNTRUST)` |
|      - |  632 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  633 | `		return SXERR_EMPTY;` |
|      - |  634 | `	}` |
|      - |  635 | `#endif` |
|     11 |  636 | `	va_start(ap,zFormat);` |
|     11 |  637 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|     11 |  638 | `	va_end(ap);` |
|     11 |  639 | `	return rc;` |
|      1 |  640 |  |
|  61554 |  641 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|      2 |  642 |  |
|      - |  643 | `	va_list ap;` |
|      - |  644 | `	sxu32 n;` |
|      - |  645 | `#if defined(UNTRUST)` |
|      - |  646 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  647 | `		return 0;` |
|      - |  648 | `	}` |
|      - |  649 | `#endif` |
|  61556 |  650 | `	va_start(ap,zFormat);` |
|  61556 |  651 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|  61556 |  652 | `	va_end(ap);` |
|  61556 |  653 | `	return n;` |
|      2 |  654 |  |
|    716 |  655 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|      2 |  656 |  |
|    718 |  657 | `	sxu32 n = 0; /* cc warning */` |
|      - |  658 | `#if defined(UNTRUST)` |
|      - |  659 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  660 | `		return 0;` |
|      - |  661 | `	}` |
|      - |  662 | `#endif` |
|    718 |  663 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    718 |  664 | `	return n;` |
|      2 |  665 |  |
|  34768 |  666 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|      2 |  667 |  |
|      - |  668 | `	SyBlob sBlob;` |
|      - |  669 | `	va_list ap;` |
|      - |  670 | `	sxu32 n;` |
|      - |  671 | `#if defined(UNTRUST)` |
|      - |  672 | `	if( SX_EMPTY_STR(zFormat) ){` |
|      - |  673 | `		return 0;` |
|      - |  674 | `	}` |
|      - |  675 | `#endif` |
|  34770 |  676 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|    ! 0 |  677 | `		return 0;` |
|      - |  678 | `	}` |
|  34770 |  679 | `	va_start(ap,zFormat);` |
|  34770 |  680 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  34770 |  681 | `	va_end(ap);` |
|  34770 |  682 | `	n = SyBlobLength(&sBlob);` |
|      - |  683 | `	/* Append the null terminator */` |
|  34770 |  684 | `	sBlob.mByte++;` |
|  34770 |  685 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  34770 |  686 | `	return n;` |
|  17386 |  687 |  |
|      - |  688 |  |
