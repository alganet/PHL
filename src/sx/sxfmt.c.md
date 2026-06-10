# src/sx/sxfmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 291/413 lines (70.46%)

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
|       - |   46 |  |
|       - |   47 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|       - |   48 | `  sxu8 base;     /* The base for radix conversion */` |
|       - |   49 | `  int flags;    /* One or more of SXFLAG_ constants below */` |
|       - |   50 | `  sxu8 type;     /* Conversion paradigm */` |
|       - |   51 | `  char *charset; /* The character set for conversion */` |
|       - |   52 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|       - |   53 | `};` |
|       - |   54 | `typedef struct SyFmtConsumer SyFmtConsumer;` |
|       - |   55 | `struct SyFmtConsumer` |
|       - |   56 |  |
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
|       - |   68 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|    4022 |   69 | `static int getdigit(sxlongreal *val,int *cnt)` |
|       2 |   70 |  |
|       - |   71 | `  sxlongreal d;` |
|       - |   72 | `  int digit;` |
|       - |   73 |  |
|    4024 |   74 | `  if( (*cnt)++ >= 16 ){` |
|      31 |   75 | `	  return '0';` |
|       - |   76 | `  }` |
|    3994 |   77 | `  digit = (int)*val;` |
|    3994 |   78 | `  d = digit;` |
|    3994 |   79 | `   *val = (*val - d)*10.0;` |
|    3994 |   80 | `  return digit + '0' ;` |
|    2013 |   81 |  |
|       - |   82 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|       - |   83 | `/*` |
|       - |   84 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|       - |   85 | ` * extended by Symisc Systems to fit its need.` |
|       - |   86 | ` * Status: Public Domain` |
|       - |   87 | ` */` |
|  300514 |   88 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
|       2 |   89 |  |
|       - |   90 | `	/*` |
|       - |   91 | `	 * The following table is searched linearly, so it is good to put the most frequently` |
|       - |   92 | `	 * used conversion types first.` |
|       - |   93 | `	 */` |
|       - |   94 | `static const SyFmtInfo aFmt[] = {` |
|       - |   95 | `  {  'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789",0    },` |
|       - |   96 | `  {  's',  0, 0, SXFMT_STRING,     0,                  0    },` |
|       - |   97 | `  {  'c',  0, 0, SXFMT_CHARX,      0,                  0    },` |
|       - |   98 | `  {  'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },` |
|       - |   99 | `  {  'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|       - |  100 | `         /* -- Extensions by Symisc Systems -- */` |
|       - |  101 | `  {  'z',  0, 0, SXFMT_RAWSTR,     0,                   0   }, /* Pointer to a raw string (SyString *) */` |
|       - |  102 | `  {  'B',  2, 0, SXFMT_RADIX,      "01",                "b0"},` |
|       - |  103 | `         /* -- End of Extensions -- */` |
|       - |  104 | `  {  'o',  8, 0, SXFMT_RADIX,      "01234567",         "0"  },` |
|       - |  105 | `  {  'u', 10, 0, SXFMT_RADIX,      "0123456789",       0    },` |
|       - |  106 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|       - |  107 | `  {  'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0    },` |
|       - |  108 | `  {  'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0    },` |
|       - |  109 | `  {  'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0    },` |
|       - |  110 | `  {  'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0    },` |
|       - |  111 | `  {  'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0    },` |
|       - |  112 | `#endif` |
|       - |  113 | `  {  'i', 10, SXFLAG_SIGNED, SXFMT_RADIX,"0123456789", 0    },` |
|       - |  114 | `  {  'n',  0, 0, SXFMT_SIZE,       0,                  0    },` |
|       - |  115 | `  {  '%',  0, 0, SXFMT_PERCENT,    0,                  0    },` |
|       - |  116 | `  {  'p', 10, 0, SXFMT_RADIX,      "0123456789",       0    }` |
|       - |  117 | `};` |
|       - |  118 | `  int c;                     /* Next character in the format string */` |
|       - |  119 | `  char *bufpt;               /* Pointer to the conversion buffer */` |
|       - |  120 | `  int precision;             /* Precision of the current field */` |
|       - |  121 | `  int length;                /* Length of the field */` |
|       - |  122 | `  int idx;                   /* A general purpose loop counter */` |
|       - |  123 | `  int width;                 /* Width of the current field */` |
|       - |  124 | `  sxu8 flag_leftjustify;   /* True if "-" flag is present */` |
|       - |  125 | `  sxu8 flag_plussign;      /* True if "+" flag is present */` |
|       - |  126 | `  sxu8 flag_blanksign;     /* True if " " flag is present */` |
|       - |  127 | `  sxu8 flag_alternateform; /* True if "#" flag is present */` |
|       - |  128 | `  sxu8 flag_zeropad;       /* True if field width constant starts with zero */` |
|       - |  129 | `  sxu8 flag_long;          /* True if "l" flag is present */` |
|       - |  130 | `  sxi64 longvalue;         /* Value for integer types */` |
|       - |  131 | `  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */` |
|       - |  132 | `  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */` |
|       - |  133 | `  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/` |
|  300516 |  134 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
|       - |  135 | `  sxu8 xtype;              /* Conversion paradigm */` |
|       - |  136 | `  static char spaces[] = "                                                  ";` |
|       - |  137 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|       - |  138 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|       - |  139 | `  sxlongreal realvalue;    /* Value for real types */` |
|       - |  140 | `  int  exp;                /* exponent of real numbers */` |
|       - |  141 | `  double rounder;          /* Used for rounding floating point values */` |
|       - |  142 | `  sxu8 flag_dp;            /* True if decimal point should be shown */` |
|       - |  143 | `  sxu8 flag_rtz;           /* True if trailing zeros should be removed */` |
|       - |  144 | `  sxu8 flag_exp;           /* True to force display of the exponent */` |
|       - |  145 | `  int nsd;                 /* Number of significant digits returned */` |
|       - |  146 | `#endif` |
|       - |  147 | `  int rc;` |
|       - |  148 |  |
|  300516 |  149 | `  length = 0;` |
|  300516 |  150 | `  bufpt = 0;` |
| 1086364 |  151 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 1021610 |  152 | `    if( c!='%' ){` |
|       - |  153 | `      unsigned int amt;` |
|  937762 |  154 | `      bufpt = (char *)zFormat;` |
|  937762 |  155 | `      amt = 1;` |
| 1483508 |  156 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
|  937762 |  157 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
|  937762 |  158 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  159 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  160 | `	  }` |
|  937762 |  161 | `      if( c==0 ){` |
|  235762 |  162 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  163 | `	  }` |
|  351000 |  164 | `    }` |
|  785850 |  165 | `    if( (c=(*++zFormat))==0 ){` |
|     ! 0 |  166 | `      errorflag = 1;` |
|     ! 0 |  167 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|     ! 0 |  168 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  169 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  170 | `	  }` |
|     ! 0 |  171 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  172 | `    }` |
|       - |  173 | `    /* Find out what flags are present */` |
|  785850 |  174 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
|  785848 |  175 | `     flag_alternateform = flag_zeropad = 0;` |
|  392924 |  176 | `    do{` |
|  785962 |  177 | `      switch( c ){` |
|     ! 0 |  178 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|       3 |  179 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  180 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      18 |  181 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      95 |  182 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
|  785848 |  183 | `        default:                                       break;` |
|       - |  184 | `      }` |
|  785962 |  185 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|       - |  186 | `    /* Get the field width */` |
|  785850 |  187 | `    width = 0;` |
|  785850 |  188 | `    if( c=='*' ){` |
|     ! 0 |  189 | `      width = va_arg(ap,int);` |
|     ! 0 |  190 | `      if( width<0 ){` |
|     ! 0 |  191 | `        flag_leftjustify = 1;` |
|     ! 0 |  192 | `        width = -width;` |
|     ! 0 |  193 | `      }` |
|     ! 0 |  194 | `      c = *++zFormat;` |
|     ! 0 |  195 | `    }else{` |
|  786004 |  196 | `      while( c>='0' && c<='9' ){` |
|     156 |  197 | `        width = width*10 + c - '0';` |
|     156 |  198 | `        c = *++zFormat;` |
|       2 |  199 | `      }` |
|       - |  200 | `    }` |
|  785850 |  201 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|     ! 0 |  202 | `      width = SXFMT_BUFSIZ-10;` |
|     ! 0 |  203 | `    }` |
|       - |  204 | `    /* Get the precision */` |
|  785850 |  205 | `	precision = -1;` |
|  785850 |  206 | `    if( c=='.' ){` |
|  233822 |  207 | `      precision = 0;` |
|  233822 |  208 | `      c = *++zFormat;` |
|  233822 |  209 | `      if( c=='*' ){` |
|  233562 |  210 | `        precision = va_arg(ap,int);` |
|  233562 |  211 | `        if( precision<0 ) precision = -precision;` |
|  233562 |  212 | `        c = *++zFormat;` |
|  116782 |  213 | `      }else{` |
|     782 |  214 | `        while( c>='0' && c<='9' ){` |
|     522 |  215 | `          precision = precision*10 + c - '0';` |
|     522 |  216 | `          c = *++zFormat;` |
|       2 |  217 | `        }` |
|       - |  218 | `      }` |
|  116910 |  219 | `    }` |
|       - |  220 | `    /* Get the conversion type modifier */` |
|  785850 |  221 | `	flag_long = 0;` |
|  785850 |  222 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|   54214 |  223 | `      flag_long = (c == 'q') ? 2 : 1;` |
|   54214 |  224 | `      c = *++zFormat;` |
|   54214 |  225 | `	  if( c == 'l' ){` |
|       - |  226 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|     ! 0 |  227 | `		  flag_long = 2;` |
|     ! 0 |  228 | `	  }` |
|   27106 |  229 | `    }` |
|       - |  230 | `    /* Fetch the info entry for the field */` |
|  785850 |  231 | `    infop = 0;` |
|  785850 |  232 | `    xtype = SXFMT_ERROR;` |
| 3476370 |  233 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 3476370 |  234 | `      if( c==aFmt[idx].fmttype ){` |
|  785850 |  235 | `        infop = &aFmt[idx];` |
|  785850 |  236 | `		xtype = infop->type;` |
|  785850 |  237 | `        break;` |
|       - |  238 | `      }` |
| 1345262 |  239 | `    }` |
|       - |  240 | `    /* zExtra is not used in this code path. */` |
|       - |  241 |  |
|       - |  242 | `    /*` |
|       - |  243 | `    ** At this point, variables are initialized as follows:` |
|       - |  244 | `    **` |
|       - |  245 | `    **   flag_alternateform          TRUE if a '#' is present.` |
|       - |  246 | `    **   flag_plussign               TRUE if a '+' is present.` |
|       - |  247 | `    **   flag_leftjustify            TRUE if a '-' is present or if the` |
|       - |  248 | `    **                               field width was negative.` |
|       - |  249 | `    **   flag_zeropad                TRUE if the width began with 0.` |
|       - |  250 | `    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed` |
|       - |  251 | `    **                               the conversion character.` |
|       - |  252 | `    **   flag_blanksign              TRUE if a ' ' is present.` |
|       - |  253 | `    **   width                       The specified field width.This is` |
|       - |  254 | `    **                               always non-negative.Zero is the default.` |
|       - |  255 | `    **   precision                   The specified precision.The default` |
|       - |  256 | `    **                               is -1.` |
|       - |  257 | `    **   xtype                       The class of the conversion.` |
|       - |  258 | `    **   infop                       Pointer to the appropriate info struct.` |
|       - |  259 | `    */` |
|  785850 |  260 | `    switch( xtype ){` |
|   28070 |  261 | `      case SXFMT_RADIX:` |
|   56142 |  262 | `        if( flag_long > 0 ){` |
|   54214 |  263 | `			if( flag_long > 1 ){` |
|       - |  264 | `				/* BSD quad: expect a 64-bit integer */` |
|   54206 |  265 | `				longvalue = va_arg(ap,sxi64);` |
|   27104 |  266 | `			}else{` |
|       9 |  267 | `				longvalue = va_arg(ap,sxlong);` |
|       - |  268 | `			}` |
|   27108 |  269 | `		}else{` |
|    1930 |  270 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|     994 |  271 | `				longvalue = va_arg(ap,sxi32);` |
|     498 |  272 | `			}else{` |
|     938 |  273 | `				longvalue = va_arg(ap,sxu32);` |
|       - |  274 | `			}` |
|       - |  275 | `		}` |
|       - |  276 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   56142 |  277 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|       - |  278 | `#if 1` |
|       - |  279 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|       - |  280 | `        ** I think this is stupid.*/` |
|   56142 |  281 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|       - |  282 | `#else` |
|       - |  283 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|       - |  284 | `        ** but leave the prefix for hex.*/` |
|       - |  285 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|       - |  286 | `#endif` |
|   56142 |  287 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|   55196 |  288 | `          if( longvalue<0 ){` |
|      96 |  289 | `            longvalue = -longvalue;` |
|       - |  290 | `			/* Ticket 1433-003 */` |
|      96 |  291 | `			if( longvalue < 0 ){` |
|       - |  292 | `				/* Overflow */` |
|     ! 0 |  293 | `				longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  294 | `			}` |
|      96 |  295 | `            prefix = '-';` |
|   55149 |  296 | `          }else if( flag_plussign )  prefix = '+';` |
|   55100 |  297 | `          else if( flag_blanksign )  prefix = ' ';` |
|   55100 |  298 | `          else                       prefix = 0;` |
|   27599 |  299 | `        }else{` |
|     948 |  300 | `			if( longvalue<0 ){` |
|     ! 0 |  301 | `				longvalue = -longvalue;` |
|       - |  302 | `				/* Ticket 1433-003 */` |
|     ! 0 |  303 | `				if( longvalue < 0 ){` |
|       - |  304 | `					/* Overflow */` |
|     ! 0 |  305 | `					longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  306 | `				}` |
|     ! 0 |  307 | `			}` |
|     948 |  308 | `			prefix = 0;` |
|       - |  309 | `		}` |
|   56142 |  310 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      95 |  311 | `          precision = width-(prefix!=0);` |
|      47 |  312 | `        }` |
|   56142 |  313 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|       - |  314 | `        {` |
|       - |  315 | `          register char *cset;      /* Use registers for speed */` |
|       - |  316 | `          register int base;` |
|   56142 |  317 | `          cset = infop->charset;` |
|   56142 |  318 | `          base = infop->base;` |
|   28070 |  319 | `          do{                                           /* Convert to ascii */` |
|  173208 |  320 | `            *(--bufpt) = cset[longvalue%base];` |
|  173208 |  321 | `            longvalue = longvalue/base;` |
|  173208 |  322 | `          }while( longvalue>0 );` |
|       - |  323 | `        }` |
|   56142 |  324 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   56200 |  325 | `        for(idx=precision-length; idx>0; idx--){` |
|      59 |  326 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|      30 |  327 | `        }` |
|   56142 |  328 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|   56142 |  329 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|       - |  330 | `          char *pre, x;` |
|       5 |  331 | `          pre = infop->prefix;` |
|       5 |  332 | `          if( *bufpt!=pre[0] ){` |
|      13 |  333 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|       2 |  334 | `          }` |
|       2 |  335 | `        }` |
|   56142 |  336 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   56142 |  337 | `        break;` |
|     136 |  338 | `      case SXFMT_FLOAT:` |
|       - |  339 | `      case SXFMT_EXP:` |
|       - |  340 | `      case SXFMT_GENERIC:` |
|       - |  341 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|     274 |  342 | `		realvalue = va_arg(ap,double);` |
|       - |  343 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|     274 |  344 | `        if( PH7_IS_NAN(realvalue) ){` |
|       - |  345 | `            /* lowercase nan consistent with libc */` |
|     ! 0 |  346 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|       - |  347 | `            /* the value has no sign; make sure prefix is clear */` |
|     ! 0 |  348 | `            prefix = 0;` |
|     ! 0 |  349 | `            bufpt = buf + 3;` |
|     ! 0 |  350 | `            goto float_done;` |
|       - |  351 | `        }` |
|     274 |  352 | `        if( PH7_IS_INF(realvalue) ){` |
|     ! 0 |  353 | `            if( realvalue < 0.0 ){` |
|       - |  354 | `                /* negative infinity should be signed via prefix */` |
|     ! 0 |  355 | `                prefix = '-';` |
|     ! 0 |  356 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  357 | `                bufpt = buf + 3;` |
|     ! 0 |  358 | `            }else{` |
|       - |  359 | `                /* positive infinity treated like a plain value */` |
|     ! 0 |  360 | `                prefix = 0;` |
|     ! 0 |  361 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  362 | `                bufpt = buf + 3;` |
|       - |  363 | `            }` |
|     ! 0 |  364 | `            goto float_done;` |
|       - |  365 | `        }` |
|     274 |  366 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     274 |  367 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|     274 |  368 | `        if( realvalue<0.0 ){` |
|      25 |  369 | `          realvalue = -realvalue;` |
|      25 |  370 | `          prefix = '-';` |
|      13 |  371 | `        }else{` |
|     250 |  372 | `          if( flag_plussign )          prefix = '+';` |
|     250 |  373 | `          else if( flag_blanksign )    prefix = ' ';` |
|     250 |  374 | `          else                         prefix = 0;` |
|       - |  375 | `        }` |
|     274 |  376 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|     274 |  377 | `        rounder = 0.0;` |
|       - |  378 | `#if 0` |
|       - |  379 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|       - |  380 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|       - |  381 | `#else` |
|       - |  382 | `        /* It makes more sense to use 0.5 */` |
|    4024 |  383 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|       - |  384 | `#endif` |
|     274 |  385 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|       - |  386 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     274 |  387 | `        exp = 0;` |
|     274 |  388 | `        if( realvalue>0.0 ){` |
|     258 |  389 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     362 |  390 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     248 |  391 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     302 |  392 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     248 |  393 | `          if( exp>350 \|\| exp<-350 ){` |
|     ! 0 |  394 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|     ! 0 |  395 | `            bufpt = buf + 3;` |
|     ! 0 |  396 | `            goto float_done;` |
|       - |  397 | `          }` |
|     123 |  398 | `        }` |
|     274 |  399 | `        bufpt = buf;` |
|       - |  400 | `        /*` |
|       - |  401 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|       - |  402 | `        ** or etFLOAT, as appropriate.` |
|       - |  403 | `        */` |
|     274 |  404 | `        flag_exp = xtype==SXFMT_EXP;` |
|     274 |  405 | `        if( xtype!=SXFMT_FLOAT ){` |
|     272 |  406 | `          realvalue += rounder;` |
|     272 |  407 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|     135 |  408 | `        }` |
|     274 |  409 | `        if( xtype==SXFMT_GENERIC ){` |
|     272 |  410 | `          flag_rtz = !flag_alternateform;` |
|     272 |  411 | `          if( exp<-4 \|\| exp>precision ){` |
|     ! 0 |  412 | `            xtype = SXFMT_EXP;` |
|     ! 0 |  413 | `          }else{` |
|     272 |  414 | `            precision = precision - exp;` |
|     272 |  415 | `            xtype = SXFMT_FLOAT;` |
|       - |  416 | `          }` |
|     137 |  417 | `        }else{` |
|       3 |  418 | `          flag_rtz = 0;` |
|       - |  419 | `        }` |
|       - |  420 | `        /*` |
|       - |  421 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|       - |  422 | `        ** the precision is too large to fit in buf[].` |
|       - |  423 | `        */` |
|     274 |  424 | `        nsd = 0;` |
|     410 |  425 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|     274 |  426 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     274 |  427 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|     274 |  428 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|     664 |  429 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     274 |  430 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|     290 |  431 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|      17 |  432 | `            *(bufpt++) = '0';` |
|       9 |  433 | `          }` |
|    3868 |  434 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     274 |  435 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|     274 |  436 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    2890 |  437 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     272 |  438 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     135 |  439 | `          }` |
|     274 |  440 | `          bufpt++;                            /* point to next free slot */` |
|     138 |  441 | `        }else{    /* etEXP or etGENERIC */` |
|     ! 0 |  442 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     ! 0 |  443 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|     ! 0 |  444 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|     ! 0 |  445 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|     ! 0 |  446 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     ! 0 |  447 | `          bufpt--;                            /* point to last digit */` |
|     ! 0 |  448 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|     ! 0 |  449 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     ! 0 |  450 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     ! 0 |  451 | `          }` |
|     ! 0 |  452 | `          bufpt++;                            /* point to next free slot */` |
|     ! 0 |  453 | `          if( exp \|\| flag_exp ){` |
|     ! 0 |  454 | `            *(bufpt++) = infop->charset[0];` |
|     ! 0 |  455 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|     ! 0 |  456 | `            else       { *(bufpt++) = '+'; }` |
|     ! 0 |  457 | `            if( exp>=100 ){` |
|     ! 0 |  458 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|     ! 0 |  459 | `              exp %= 100;` |
|     ! 0 |  460 | `            }` |
|     ! 0 |  461 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|     ! 0 |  462 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|     ! 0 |  463 | `          }` |
|       - |  464 | `        }` |
|     ! 0 |  465 | `        float_done:` |
|       - |  466 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|       - |  467 | `        ** Note that the number is in the usual order, not reversed as with` |
|       - |  468 | `        ** integer conversions.*/` |
|     274 |  469 | `        length = (int)(bufpt-buf);` |
|     274 |  470 | `        bufpt = buf;` |
|       - |  471 |  |
|       - |  472 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|       - |  473 | `        ** set and we are not left justified */` |
|     274 |  474 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|       - |  475 | `          int i;` |
|     ! 0 |  476 | `          int nPad = width - length;` |
|     ! 0 |  477 | `          for(i=width; i>=nPad; i--){` |
|     ! 0 |  478 | `            bufpt[i] = bufpt[i-nPad];` |
|     ! 0 |  479 | `          }` |
|     ! 0 |  480 | `          i = prefix!=0;` |
|     ! 0 |  481 | `          while( nPad-- ) bufpt[i++] = '0';` |
|     ! 0 |  482 | `          length = width;` |
|     ! 0 |  483 | `        }` |
|       - |  484 | `#else` |
|       - |  485 | `         bufpt = " ";` |
|       - |  486 | `		 length = (int)sizeof(" ") - 1;` |
|       - |  487 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|     274 |  488 | `        break;` |
|     ! 0 |  489 | `      case SXFMT_SIZE:{` |
|     ! 0 |  490 | `		 int *pSize = va_arg(ap,int *);` |
|     ! 0 |  491 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|     ! 0 |  492 | `		 length = width = 0;` |
|       - |  493 | `					  }` |
|     ! 0 |  494 | `        break;` |
|       1 |  495 | `      case SXFMT_PERCENT:` |
|       3 |  496 | `        buf[0] = '%';` |
|       3 |  497 | `        bufpt = buf;` |
|       3 |  498 | `        length = 1;` |
|       3 |  499 | `        break;` |
|    4752 |  500 | `      case SXFMT_CHARX:` |
|    9506 |  501 | `        c = va_arg(ap,int);` |
|    9506 |  502 | `		buf[0] = (char)c;` |
|       - |  503 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    9506 |  504 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|    9506 |  505 | `        if( precision>=0 ){` |
|     ! 0 |  506 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|     ! 0 |  507 | `          length = precision;` |
|     ! 0 |  508 | `        }else{` |
|    9506 |  509 | `          length =1;` |
|       - |  510 | `        }` |
|    9506 |  511 | `        bufpt = buf;` |
|    9506 |  512 | `        break;` |
|  117355 |  513 | `      case SXFMT_STRING:` |
|  234712 |  514 | `        bufpt = va_arg(ap,char*);` |
|  234712 |  515 | `        if( bufpt==0 ){` |
|     ! 0 |  516 | `          bufpt = " ";` |
|     ! 0 |  517 | `		  length = (int)sizeof(" ")-1;` |
|     ! 0 |  518 | `		  break;` |
|       - |  519 | `        }` |
|  234712 |  520 | `		length = precision;` |
|  234712 |  521 | `		if( precision < 0 ){` |
|       - |  522 | `			/* Symisc extension */` |
|    1154 |  523 | `			length = (int)SyStrlen(bufpt);` |
|     576 |  524 | `		}` |
|  234712 |  525 | `        if( precision>=0 && precision<length ) length = precision;` |
|  234712 |  526 | `        break;` |
|  242610 |  527 | `	case SXFMT_RAWSTR:{` |
|       - |  528 | `		/* Symisc extension */` |
|  485222 |  529 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  485222 |  530 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|     ! 0 |  531 | `			 bufpt = " ";` |
|     ! 0 |  532 | `		     length = (int)sizeof(char);` |
|     ! 0 |  533 | `		     break;` |
|       - |  534 | `		}` |
|  485222 |  535 | `		bufpt = (char *)pStr->zString;` |
|  485222 |  536 | `		length = (int)pStr->nByte;` |
|  485222 |  537 | `		break;` |
|       - |  538 | `					  }` |
|     ! 0 |  539 | `      case SXFMT_ERROR:` |
|     ! 0 |  540 | `        buf[0] = '?';` |
|     ! 0 |  541 | `        bufpt = buf;` |
|     ! 0 |  542 | `		length = (int)sizeof(char);` |
|     ! 0 |  543 | `        if( c==0 ) zFormat--;` |
|     ! 0 |  544 | `        break;` |
|       - |  545 | `    }/* End switch over the format type */` |
|       - |  546 | `    /*` |
|       - |  547 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|       - |  548 | `    ** "length" characters long.The field width is "width".Do` |
|       - |  549 | `    ** the output.` |
|       - |  550 | `    */` |
|  785850 |  551 | `    if( !flag_leftjustify ){` |
|       - |  552 | `      register int nspace;` |
|  785850 |  553 | `      nspace = width-length;` |
|  785850 |  554 | `      if( nspace>0 ){` |
|      37 |  555 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  556 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  557 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  558 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  559 | `			}` |
|     ! 0 |  560 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  561 | `        }` |
|      37 |  562 | `        if( nspace>0 ){` |
|      37 |  563 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|      37 |  564 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  565 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  566 | `			}` |
|      18 |  567 | `		}` |
|      18 |  568 | `      }` |
|  392924 |  569 | `    }` |
|  785850 |  570 | `    if( length>0 ){` |
|  785826 |  571 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
|  785826 |  572 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  573 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  574 | `		}` |
|  392912 |  575 | `    }` |
|  785850 |  576 | `    if( flag_leftjustify ){` |
|       - |  577 | `      register int nspace;` |
|     ! 0 |  578 | `      nspace = width-length;` |
|     ! 0 |  579 | `      if( nspace>0 ){` |
|     ! 0 |  580 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  581 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  582 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  583 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  584 | `			}` |
|     ! 0 |  585 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  586 | `        }` |
|     ! 0 |  587 | `        if( nspace>0 ){` |
|     ! 0 |  588 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|     ! 0 |  589 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  590 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  591 | `			}` |
|     ! 0 |  592 | `		}` |
|     ! 0 |  593 | `      }` |
|     ! 0 |  594 | `    }` |
|  392926 |  595 | `  }/* End for loop over the format string */` |
|   64756 |  596 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  150259 |  597 |  |
| 1723620 |  598 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|       2 |  599 |  |
| 1723622 |  600 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 1723622 |  601 | `	sxi32 rc = SXERR_ABORT;` |
| 1723622 |  602 | `	switch(pConsumer->nType){` |
|      78 |  603 | `	case SXFMT_CONS_PROC:` |
|       - |  604 | `			/* User callback */` |
|     157 |  605 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|     157 |  606 | `			break;` |
|  861732 |  607 | `	case SXFMT_CONS_BLOB:` |
|       - |  608 | `			/* Blob consumer */` |
| 1723466 |  609 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 1723464 |  610 | `			break;` |
|     ! 0 |  611 | `		default:` |
|       - |  612 | `			/* Unknown consumer */` |
|     ! 0 |  613 | `			break;` |
|       - |  614 | `	}` |
|       - |  615 | `	/* Update total number of bytes consumed so far */` |
| 1723622 |  616 | `	pConsumer->nLen += nLen;` |
| 1723622 |  617 | `	pConsumer->rc = rc;` |
| 1723622 |  618 | `	return rc;` |
|       2 |  619 |  |
|  300514 |  620 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|       2 |  621 |  |
|       - |  622 | `	SyFmtConsumer sCons;` |
|  300516 |  623 | `	sCons.nType = nType;` |
|  300516 |  624 | `	sCons.rc = SXRET_OK;` |
|  300516 |  625 | `	sCons.nLen = 0;` |
|  300516 |  626 | `	if( pOutLen ){` |
|   66992 |  627 | `		*pOutLen = 0;` |
|   33495 |  628 | `	}` |
|  300516 |  629 | `	switch(nType){` |
|       6 |  630 | `	case SXFMT_CONS_PROC:` |
|       - |  631 | `#if defined(UNTRUST)` |
|       - |  632 | `			if( xUserCons == 0 ){` |
|       - |  633 | `				return SXERR_EMPTY;` |
|       - |  634 | `			}` |
|       - |  635 | `#endif` |
|      13 |  636 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|      13 |  637 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|      13 |  638 | `		break;` |
|  150251 |  639 | `		case SXFMT_CONS_BLOB:` |
|  300504 |  640 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  300504 |  641 | `			break;` |
|     ! 0 |  642 | `		default:` |
|     ! 0 |  643 | `			return SXERR_UNKNOWN;` |
|       - |  644 | `	}` |
|  300516 |  645 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  300516 |  646 | `	if( pOutLen ){` |
|   66992 |  647 | `		*pOutLen = sCons.nLen;` |
|   33495 |  648 | `	}` |
|  300516 |  649 | `	return sCons.rc;` |
|  150259 |  650 |  |
|      12 |  651 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|       1 |  652 |  |
|       - |  653 | `	va_list ap;` |
|       - |  654 | `	sxi32 rc;` |
|       - |  655 | `#if defined(UNTRUST)` |
|       - |  656 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  657 | `		return SXERR_EMPTY;` |
|       - |  658 | `	}` |
|       - |  659 | `#endif` |
|      13 |  660 | `	va_start(ap,zFormat);` |
|      13 |  661 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|      13 |  662 | `	va_end(ap);` |
|      13 |  663 | `	return rc;` |
|       1 |  664 |  |
|   65556 |  665 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|       2 |  666 |  |
|       - |  667 | `	va_list ap;` |
|       - |  668 | `	sxu32 n;` |
|       - |  669 | `#if defined(UNTRUST)` |
|       - |  670 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  671 | `		return 0;` |
|       - |  672 | `	}` |
|       - |  673 | `#endif` |
|   65558 |  674 | `	va_start(ap,zFormat);` |
|   65558 |  675 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|   65558 |  676 | `	va_end(ap);` |
|   65558 |  677 | `	return n;` |
|       2 |  678 |  |
|    1434 |  679 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|       2 |  680 |  |
|    1436 |  681 | `	sxu32 n = 0; /* cc warning */` |
|       - |  682 | `#if defined(UNTRUST)` |
|       - |  683 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  684 | `		return 0;` |
|       - |  685 | `	}` |
|       - |  686 | `#endif` |
|    1436 |  687 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    1436 |  688 | `	return n;` |
|       2 |  689 |  |
|  233512 |  690 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|       2 |  691 |  |
|       - |  692 | `	SyBlob sBlob;` |
|       - |  693 | `	va_list ap;` |
|       - |  694 | `	sxu32 n;` |
|       - |  695 | `#if defined(UNTRUST)` |
|       - |  696 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  697 | `		return 0;` |
|       - |  698 | `	}` |
|       - |  699 | `#endif` |
|  233514 |  700 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|     ! 0 |  701 | `		return 0;` |
|       - |  702 | `	}` |
|  233514 |  703 | `	va_start(ap,zFormat);` |
|  233514 |  704 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  233514 |  705 | `	va_end(ap);` |
|  233514 |  706 | `	n = SyBlobLength(&sBlob);` |
|       - |  707 | `	/* Append the null terminator */` |
|  233514 |  708 | `	sBlob.mByte++;` |
|  233514 |  709 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  233514 |  710 | `	return n;` |
|  116758 |  711 |  |
|       - |  712 |  |
