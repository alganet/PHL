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
|    2522 |   69 | `static int getdigit(sxlongreal *val,int *cnt)` |
|       2 |   70 |  |
|       - |   71 | `  sxlongreal d;` |
|       - |   72 | `  int digit;` |
|       - |   73 |  |
|    2524 |   74 | `  if( (*cnt)++ >= 16 ){` |
|      31 |   75 | `	  return '0';` |
|       - |   76 | `  }` |
|    2494 |   77 | `  digit = (int)*val;` |
|    2494 |   78 | `  d = digit;` |
|    2494 |   79 | `   *val = (*val - d)*10.0;` |
|    2494 |   80 | `  return digit + '0' ;` |
|    1263 |   81 |  |
|       - |   82 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|       - |   83 | `/*` |
|       - |   84 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|       - |   85 | ` * extended by Symisc Systems to fit its need.` |
|       - |   86 | ` * Status: Public Domain` |
|       - |   87 | ` */` |
|  200458 |   88 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
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
|  200460 |  134 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
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
|  200460 |  149 | `  length = 0;` |
|  200460 |  150 | `  bufpt = 0;` |
|  690782 |  151 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
|  629550 |  152 | `    if( c!='%' ){` |
|       - |  153 | `      unsigned int amt;` |
|  552862 |  154 | `      bufpt = (char *)zFormat;` |
|  552862 |  155 | `      amt = 1;` |
|  887366 |  156 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
|  552862 |  157 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
|  552862 |  158 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  159 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  160 | `	  }` |
|  552862 |  161 | `      if( c==0 ){` |
|  139228 |  162 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  163 | `	  }` |
|  206817 |  164 | `    }` |
|  490324 |  165 | `    if( (c=(*++zFormat))==0 ){` |
|     ! 0 |  166 | `      errorflag = 1;` |
|     ! 0 |  167 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|     ! 0 |  168 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  169 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  170 | `	  }` |
|     ! 0 |  171 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  172 | `    }` |
|       - |  173 | `    /* Find out what flags are present */` |
|  490324 |  174 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
|  490322 |  175 | `     flag_alternateform = flag_zeropad = 0;` |
|  245161 |  176 | `    do{` |
|  490436 |  177 | `      switch( c ){` |
|     ! 0 |  178 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|       3 |  179 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  180 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      18 |  181 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|      95 |  182 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
|  490322 |  183 | `        default:                                       break;` |
|       - |  184 | `      }` |
|  490436 |  185 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|       - |  186 | `    /* Get the field width */` |
|  490324 |  187 | `    width = 0;` |
|  490324 |  188 | `    if( c=='*' ){` |
|     ! 0 |  189 | `      width = va_arg(ap,int);` |
|     ! 0 |  190 | `      if( width<0 ){` |
|     ! 0 |  191 | `        flag_leftjustify = 1;` |
|     ! 0 |  192 | `        width = -width;` |
|     ! 0 |  193 | `      }` |
|     ! 0 |  194 | `      c = *++zFormat;` |
|     ! 0 |  195 | `    }else{` |
|  490478 |  196 | `      while( c>='0' && c<='9' ){` |
|     156 |  197 | `        width = width*10 + c - '0';` |
|     156 |  198 | `        c = *++zFormat;` |
|       2 |  199 | `      }` |
|       - |  200 | `    }` |
|  490324 |  201 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|     ! 0 |  202 | `      width = SXFMT_BUFSIZ-10;` |
|     ! 0 |  203 | `    }` |
|       - |  204 | `    /* Get the precision */` |
|  490324 |  205 | `	precision = -1;` |
|  490324 |  206 | `    if( c=='.' ){` |
|  137838 |  207 | `      precision = 0;` |
|  137838 |  208 | `      c = *++zFormat;` |
|  137838 |  209 | `      if( c=='*' ){` |
|  137678 |  210 | `        precision = va_arg(ap,int);` |
|  137678 |  211 | `        if( precision<0 ) precision = -precision;` |
|  137678 |  212 | `        c = *++zFormat;` |
|   68840 |  213 | `      }else{` |
|     482 |  214 | `        while( c>='0' && c<='9' ){` |
|     322 |  215 | `          precision = precision*10 + c - '0';` |
|     322 |  216 | `          c = *++zFormat;` |
|       2 |  217 | `        }` |
|       - |  218 | `      }` |
|   68918 |  219 | `    }` |
|       - |  220 | `    /* Get the conversion type modifier */` |
|  490324 |  221 | `	flag_long = 0;` |
|  490324 |  222 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|   52800 |  223 | `      flag_long = (c == 'q') ? 2 : 1;` |
|   52800 |  224 | `      c = *++zFormat;` |
|   52800 |  225 | `	  if( c == 'l' ){` |
|       - |  226 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|     ! 0 |  227 | `		  flag_long = 2;` |
|     ! 0 |  228 | `	  }` |
|   26399 |  229 | `    }` |
|       - |  230 | `    /* Fetch the info entry for the field */` |
|  490324 |  231 | `    infop = 0;` |
|  490324 |  232 | `    xtype = SXFMT_ERROR;` |
| 2101850 |  233 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 2101850 |  234 | `      if( c==aFmt[idx].fmttype ){` |
|  490324 |  235 | `        infop = &aFmt[idx];` |
|  490324 |  236 | `		xtype = infop->type;` |
|  490324 |  237 | `        break;` |
|       - |  238 | `      }` |
|  805765 |  239 | `    }` |
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
|  490324 |  260 | `    switch( xtype ){` |
|   27038 |  261 | `      case SXFMT_RADIX:` |
|   54078 |  262 | `        if( flag_long > 0 ){` |
|   52800 |  263 | `			if( flag_long > 1 ){` |
|       - |  264 | `				/* BSD quad: expect a 64-bit integer */` |
|   52792 |  265 | `				longvalue = va_arg(ap,sxi64);` |
|   26397 |  266 | `			}else{` |
|       9 |  267 | `				longvalue = va_arg(ap,sxlong);` |
|       - |  268 | `			}` |
|   26401 |  269 | `		}else{` |
|    1280 |  270 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|     540 |  271 | `				longvalue = va_arg(ap,sxi32);` |
|     271 |  272 | `			}else{` |
|     742 |  273 | `				longvalue = va_arg(ap,sxu32);` |
|       - |  274 | `			}` |
|       - |  275 | `		}` |
|       - |  276 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   54078 |  277 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|       - |  278 | `#if 1` |
|       - |  279 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|       - |  280 | `        ** I think this is stupid.*/` |
|   54078 |  281 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|       - |  282 | `#else` |
|       - |  283 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|       - |  284 | `        ** but leave the prefix for hex.*/` |
|       - |  285 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|       - |  286 | `#endif` |
|   54078 |  287 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|   53328 |  288 | `          if( longvalue<0 ){` |
|      58 |  289 | `            longvalue = -longvalue;` |
|       - |  290 | `			/* Ticket 1433-003 */` |
|      58 |  291 | `			if( longvalue < 0 ){` |
|       - |  292 | `				/* Overflow */` |
|     ! 0 |  293 | `				longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  294 | `			}` |
|      58 |  295 | `            prefix = '-';` |
|   53300 |  296 | `          }else if( flag_plussign )  prefix = '+';` |
|   53270 |  297 | `          else if( flag_blanksign )  prefix = ' ';` |
|   53270 |  298 | `          else                       prefix = 0;` |
|   26665 |  299 | `        }else{` |
|     752 |  300 | `			if( longvalue<0 ){` |
|     ! 0 |  301 | `				longvalue = -longvalue;` |
|       - |  302 | `				/* Ticket 1433-003 */` |
|     ! 0 |  303 | `				if( longvalue < 0 ){` |
|       - |  304 | `					/* Overflow */` |
|     ! 0 |  305 | `					longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  306 | `				}` |
|     ! 0 |  307 | `			}` |
|     752 |  308 | `			prefix = 0;` |
|       - |  309 | `		}` |
|   54078 |  310 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      95 |  311 | `          precision = width-(prefix!=0);` |
|      47 |  312 | `        }` |
|   54078 |  313 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|       - |  314 | `        {` |
|       - |  315 | `          register char *cset;      /* Use registers for speed */` |
|       - |  316 | `          register int base;` |
|   54078 |  317 | `          cset = infop->charset;` |
|   54078 |  318 | `          base = infop->base;` |
|   27038 |  319 | `          do{                                           /* Convert to ascii */` |
|  169788 |  320 | `            *(--bufpt) = cset[longvalue%base];` |
|  169788 |  321 | `            longvalue = longvalue/base;` |
|  169788 |  322 | `          }while( longvalue>0 );` |
|       - |  323 | `        }` |
|   54078 |  324 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   54152 |  325 | `        for(idx=precision-length; idx>0; idx--){` |
|      75 |  326 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|      38 |  327 | `        }` |
|   54078 |  328 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|   54078 |  329 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|       - |  330 | `          char *pre, x;` |
|       5 |  331 | `          pre = infop->prefix;` |
|       5 |  332 | `          if( *bufpt!=pre[0] ){` |
|      13 |  333 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|       2 |  334 | `          }` |
|       2 |  335 | `        }` |
|   54078 |  336 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   54078 |  337 | `        break;` |
|      86 |  338 | `      case SXFMT_FLOAT:` |
|       - |  339 | `      case SXFMT_EXP:` |
|       - |  340 | `      case SXFMT_GENERIC:` |
|       - |  341 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|     174 |  342 | `		realvalue = va_arg(ap,double);` |
|       - |  343 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|     174 |  344 | `        if( PH7_IS_NAN(realvalue) ){` |
|       - |  345 | `            /* lowercase nan consistent with libc */` |
|     ! 0 |  346 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|       - |  347 | `            /* the value has no sign; make sure prefix is clear */` |
|     ! 0 |  348 | `            prefix = 0;` |
|     ! 0 |  349 | `            bufpt = buf + 3;` |
|     ! 0 |  350 | `            goto float_done;` |
|       - |  351 | `        }` |
|     174 |  352 | `        if( PH7_IS_INF(realvalue) ){` |
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
|     174 |  366 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     174 |  367 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|     174 |  368 | `        if( realvalue<0.0 ){` |
|      24 |  369 | `          realvalue = -realvalue;` |
|      24 |  370 | `          prefix = '-';` |
|      13 |  371 | `        }else{` |
|     152 |  372 | `          if( flag_plussign )          prefix = '+';` |
|     152 |  373 | `          else if( flag_blanksign )    prefix = ' ';` |
|     152 |  374 | `          else                         prefix = 0;` |
|       - |  375 | `        }` |
|     174 |  376 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|     174 |  377 | `        rounder = 0.0;` |
|       - |  378 | `#if 0` |
|       - |  379 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|       - |  380 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|       - |  381 | `#else` |
|       - |  382 | `        /* It makes more sense to use 0.5 */` |
|    2524 |  383 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|       - |  384 | `#endif` |
|     174 |  385 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|       - |  386 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     174 |  387 | `        exp = 0;` |
|     174 |  388 | `        if( realvalue>0.0 ){` |
|     158 |  389 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     198 |  390 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     154 |  391 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     196 |  392 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     154 |  393 | `          if( exp>350 \|\| exp<-350 ){` |
|     ! 0 |  394 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|     ! 0 |  395 | `            bufpt = buf + 3;` |
|     ! 0 |  396 | `            goto float_done;` |
|       - |  397 | `          }` |
|      76 |  398 | `        }` |
|     174 |  399 | `        bufpt = buf;` |
|       - |  400 | `        /*` |
|       - |  401 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|       - |  402 | `        ** or etFLOAT, as appropriate.` |
|       - |  403 | `        */` |
|     174 |  404 | `        flag_exp = xtype==SXFMT_EXP;` |
|     174 |  405 | `        if( xtype!=SXFMT_FLOAT ){` |
|     172 |  406 | `          realvalue += rounder;` |
|     172 |  407 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|      85 |  408 | `        }` |
|     174 |  409 | `        if( xtype==SXFMT_GENERIC ){` |
|     172 |  410 | `          flag_rtz = !flag_alternateform;` |
|     172 |  411 | `          if( exp<-4 \|\| exp>precision ){` |
|     ! 0 |  412 | `            xtype = SXFMT_EXP;` |
|     ! 0 |  413 | `          }else{` |
|     172 |  414 | `            precision = precision - exp;` |
|     172 |  415 | `            xtype = SXFMT_FLOAT;` |
|       - |  416 | `          }` |
|      87 |  417 | `        }else{` |
|       3 |  418 | `          flag_rtz = 0;` |
|       - |  419 | `        }` |
|       - |  420 | `        /*` |
|       - |  421 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|       - |  422 | `        ** the precision is too large to fit in buf[].` |
|       - |  423 | `        */` |
|     174 |  424 | `        nsd = 0;` |
|     260 |  425 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|     174 |  426 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     174 |  427 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|     174 |  428 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|     362 |  429 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     174 |  430 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|     186 |  431 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|      13 |  432 | `            *(bufpt++) = '0';` |
|       7 |  433 | `          }` |
|    2478 |  434 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     174 |  435 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|     174 |  436 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    1610 |  437 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     172 |  438 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|      85 |  439 | `          }` |
|     174 |  440 | `          bufpt++;                            /* point to next free slot */` |
|      88 |  441 | `        }else{    /* etEXP or etGENERIC */` |
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
|     174 |  469 | `        length = (int)(bufpt-buf);` |
|     174 |  470 | `        bufpt = buf;` |
|       - |  471 |  |
|       - |  472 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|       - |  473 | `        ** set and we are not left justified */` |
|     174 |  474 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
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
|     174 |  488 | `        break;` |
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
|    3849 |  500 | `      case SXFMT_CHARX:` |
|    7700 |  501 | `        c = va_arg(ap,int);` |
|    7700 |  502 | `		buf[0] = (char)c;` |
|       - |  503 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    7700 |  504 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|    7700 |  505 | `        if( precision>=0 ){` |
|     ! 0 |  506 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|     ! 0 |  507 | `          length = precision;` |
|     ! 0 |  508 | `        }else{` |
|    7700 |  509 | `          length =1;` |
|       - |  510 | `        }` |
|    7700 |  511 | `        bufpt = buf;` |
|    7700 |  512 | `        break;` |
|   69211 |  513 | `      case SXFMT_STRING:` |
|  138424 |  514 | `        bufpt = va_arg(ap,char*);` |
|  138424 |  515 | `        if( bufpt==0 ){` |
|     ! 0 |  516 | `          bufpt = " ";` |
|     ! 0 |  517 | `		  length = (int)sizeof(" ")-1;` |
|     ! 0 |  518 | `		  break;` |
|       - |  519 | `        }` |
|  138424 |  520 | `		length = precision;` |
|  138424 |  521 | `		if( precision < 0 ){` |
|       - |  522 | `			/* Symisc extension */` |
|     750 |  523 | `			length = (int)SyStrlen(bufpt);` |
|     374 |  524 | `		}` |
|  138424 |  525 | `        if( precision>=0 && precision<length ) length = precision;` |
|  138424 |  526 | `        break;` |
|  144976 |  527 | `	case SXFMT_RAWSTR:{` |
|       - |  528 | `		/* Symisc extension */` |
|  289954 |  529 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  289954 |  530 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|     ! 0 |  531 | `			 bufpt = " ";` |
|     ! 0 |  532 | `		     length = (int)sizeof(char);` |
|     ! 0 |  533 | `		     break;` |
|       - |  534 | `		}` |
|  289954 |  535 | `		bufpt = (char *)pStr->zString;` |
|  289954 |  536 | `		length = (int)pStr->nByte;` |
|  289954 |  537 | `		break;` |
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
|  490324 |  551 | `    if( !flag_leftjustify ){` |
|       - |  552 | `      register int nspace;` |
|  490324 |  553 | `      nspace = width-length;` |
|  490324 |  554 | `      if( nspace>0 ){` |
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
|  245161 |  569 | `    }` |
|  490324 |  570 | `    if( length>0 ){` |
|  490300 |  571 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
|  490300 |  572 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  573 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  574 | `		}` |
|  245149 |  575 | `    }` |
|  490324 |  576 | `    if( flag_leftjustify ){` |
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
|  245163 |  595 | `  }/* End for loop over the format string */` |
|   61234 |  596 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  100231 |  597 |  |
| 1043194 |  598 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|       2 |  599 |  |
| 1043196 |  600 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 1043196 |  601 | `	sxi32 rc = SXERR_ABORT;` |
| 1043196 |  602 | `	switch(pConsumer->nType){` |
|      78 |  603 | `	case SXFMT_CONS_PROC:` |
|       - |  604 | `			/* User callback */` |
|     157 |  605 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|     157 |  606 | `			break;` |
|  521519 |  607 | `	case SXFMT_CONS_BLOB:` |
|       - |  608 | `			/* Blob consumer */` |
| 1043040 |  609 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 1043038 |  610 | `			break;` |
|     ! 0 |  611 | `		default:` |
|       - |  612 | `			/* Unknown consumer */` |
|     ! 0 |  613 | `			break;` |
|       - |  614 | `	}` |
|       - |  615 | `	/* Update total number of bytes consumed so far */` |
| 1043196 |  616 | `	pConsumer->nLen += nLen;` |
| 1043196 |  617 | `	pConsumer->rc = rc;` |
| 1043196 |  618 | `	return rc;` |
|       2 |  619 |  |
|  200458 |  620 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|       2 |  621 |  |
|       - |  622 | `	SyFmtConsumer sCons;` |
|  200460 |  623 | `	sCons.nType = nType;` |
|  200460 |  624 | `	sCons.rc = SXRET_OK;` |
|  200460 |  625 | `	sCons.nLen = 0;` |
|  200460 |  626 | `	if( pOutLen ){` |
|   63078 |  627 | `		*pOutLen = 0;` |
|   31538 |  628 | `	}` |
|  200460 |  629 | `	switch(nType){` |
|       6 |  630 | `	case SXFMT_CONS_PROC:` |
|       - |  631 | `#if defined(UNTRUST)` |
|       - |  632 | `			if( xUserCons == 0 ){` |
|       - |  633 | `				return SXERR_EMPTY;` |
|       - |  634 | `			}` |
|       - |  635 | `#endif` |
|      13 |  636 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|      13 |  637 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|      13 |  638 | `		break;` |
|  100223 |  639 | `		case SXFMT_CONS_BLOB:` |
|  200448 |  640 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  200448 |  641 | `			break;` |
|     ! 0 |  642 | `		default:` |
|     ! 0 |  643 | `			return SXERR_UNKNOWN;` |
|       - |  644 | `	}` |
|  200460 |  645 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  200460 |  646 | `	if( pOutLen ){` |
|   63078 |  647 | `		*pOutLen = sCons.nLen;` |
|   31538 |  648 | `	}` |
|  200460 |  649 | `	return sCons.rc;` |
|  100231 |  650 |  |
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
|   61874 |  665 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|       2 |  666 |  |
|       - |  667 | `	va_list ap;` |
|       - |  668 | `	sxu32 n;` |
|       - |  669 | `#if defined(UNTRUST)` |
|       - |  670 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  671 | `		return 0;` |
|       - |  672 | `	}` |
|       - |  673 | `#endif` |
|   61876 |  674 | `	va_start(ap,zFormat);` |
|   61876 |  675 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|   61876 |  676 | `	va_end(ap);` |
|   61876 |  677 | `	return n;` |
|       2 |  678 |  |
|    1202 |  679 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|       2 |  680 |  |
|    1204 |  681 | `	sxu32 n = 0; /* cc warning */` |
|       - |  682 | `#if defined(UNTRUST)` |
|       - |  683 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  684 | `		return 0;` |
|       - |  685 | `	}` |
|       - |  686 | `#endif` |
|    1204 |  687 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    1204 |  688 | `	return n;` |
|       2 |  689 |  |
|  137370 |  690 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|       2 |  691 |  |
|       - |  692 | `	SyBlob sBlob;` |
|       - |  693 | `	va_list ap;` |
|       - |  694 | `	sxu32 n;` |
|       - |  695 | `#if defined(UNTRUST)` |
|       - |  696 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  697 | `		return 0;` |
|       - |  698 | `	}` |
|       - |  699 | `#endif` |
|  137372 |  700 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|     ! 0 |  701 | `		return 0;` |
|       - |  702 | `	}` |
|  137372 |  703 | `	va_start(ap,zFormat);` |
|  137372 |  704 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  137372 |  705 | `	va_end(ap);` |
|  137372 |  706 | `	n = SyBlobLength(&sBlob);` |
|       - |  707 | `	/* Append the null terminator */` |
|  137372 |  708 | `	sBlob.mByte++;` |
|  137372 |  709 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  137372 |  710 | `	return n;` |
|   68687 |  711 |  |
|       - |  712 |  |
