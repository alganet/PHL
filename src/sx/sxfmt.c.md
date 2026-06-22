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
|       - |   68 | `/* SPDX-SnippetBegin */` |
|       - |   69 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - |   70 | `/* SPDX-License-Identifier: blessing */` |
|       - |   71 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|    4082 |   72 | `static int getdigit(sxlongreal *val,int *cnt)` |
|       3 |   73 |  |
|       - |   74 | `  sxlongreal d;` |
|       - |   75 | `  int digit;` |
|       - |   76 |  |
|    4085 |   77 | `  if( (*cnt)++ >= 16 ){` |
|      31 |   78 | `	  return '0';` |
|       - |   79 | `  }` |
|    4055 |   80 | `  digit = (int)*val;` |
|    4055 |   81 | `  d = digit;` |
|    4055 |   82 | `   *val = (*val - d)*10.0;` |
|    4055 |   83 | `  return digit + '0' ;` |
|    2044 |   84 |  |
|       - |   85 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|       - |   86 | `/*` |
|       - |   87 | ` * The following routine was taken from the SQLITE2 source tree and was` |
|       - |   88 | ` * extended by Symisc Systems to fit its need.` |
|       - |   89 | ` * Status: Public Domain` |
|       - |   90 | ` */` |
|  315594 |   91 | `static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)` |
|       5 |   92 |  |
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
|       - |  134 | `  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */` |
|       - |  135 | `  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */` |
|       - |  136 | `  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/` |
|  315599 |  137 | `  sxu8 errorflag = 0;      /* True if an error is encountered */` |
|       - |  138 | `  sxu8 xtype;              /* Conversion paradigm */` |
|       - |  139 | `  static char spaces[] = "                                                  ";` |
|       - |  140 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|       - |  141 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|       - |  142 | `  sxlongreal realvalue;    /* Value for real types */` |
|       - |  143 | `  int  exp;                /* exponent of real numbers */` |
|       - |  144 | `  double rounder;          /* Used for rounding floating point values */` |
|       - |  145 | `  sxu8 flag_dp;            /* True if decimal point should be shown */` |
|       - |  146 | `  sxu8 flag_rtz;           /* True if trailing zeros should be removed */` |
|       - |  147 | `  sxu8 flag_exp;           /* True to force display of the exponent */` |
|       - |  148 | `  int nsd;                 /* Number of significant digits returned */` |
|       - |  149 | `#endif` |
|       - |  150 | `  int rc;` |
|       - |  151 |  |
|  315599 |  152 | `  length = 0;` |
|  315599 |  153 | `  bufpt = 0;` |
| 1145621 |  154 | `  for(; (c=(*zFormat))!=0; ++zFormat){` |
| 1080183 |  155 | `    if( c!='%' ){` |
|       - |  156 | `      unsigned int amt;` |
|  995067 |  157 | `      bufpt = (char *)zFormat;` |
|  995067 |  158 | `      amt = 1;` |
| 1573055 |  159 | `      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;` |
|  995067 |  160 | `	  rc = xConsumer((const void *)bufpt,amt,pUserData);` |
|  995067 |  161 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  162 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  163 | `	  }` |
|  995067 |  164 | `      if( c==0 ){` |
|  250161 |  165 | `		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  166 | `	  }` |
|  372453 |  167 | `    }` |
|  830027 |  168 | `    if( (c=(*++zFormat))==0 ){` |
|     ! 0 |  169 | `      errorflag = 1;` |
|     ! 0 |  170 | `	  rc = xConsumer("%",sizeof("%")-1,pUserData);` |
|     ! 0 |  171 | `	  if( rc != SXRET_OK ){` |
|     ! 0 |  172 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  173 | `	  }` |
|     ! 0 |  174 | `      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;` |
|       - |  175 | `    }` |
|       - |  176 | `    /* Find out what flags are present */` |
|  830027 |  177 | `    flag_leftjustify = flag_plussign = flag_blanksign =` |
|  830022 |  178 | `     flag_alternateform = flag_zeropad = 0;` |
|  415011 |  179 | `    do{` |
|  830145 |  180 | `      switch( c ){` |
|     ! 0 |  181 | `        case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|       3 |  182 | `        case '+':   flag_plussign = 1;        c = 0;   break;` |
|     ! 0 |  183 | `        case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      18 |  184 | `        case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     101 |  185 | `        case '0':   flag_zeropad = 1;         c = 0;   break;` |
|  830022 |  186 | `        default:                                       break;` |
|       - |  187 | `      }` |
|  830145 |  188 | `    }while( c==0 && (c=(*++zFormat))!=0 );` |
|       - |  189 | `    /* Get the field width */` |
|  830027 |  190 | `    width = 0;` |
|  830027 |  191 | `    if( c=='*' ){` |
|     ! 0 |  192 | `      width = va_arg(ap,int);` |
|     ! 0 |  193 | `      if( width<0 ){` |
|     ! 0 |  194 | `        flag_leftjustify = 1;` |
|     ! 0 |  195 | `        width = -width;` |
|     ! 0 |  196 | `      }` |
|     ! 0 |  197 | `      c = *++zFormat;` |
|     ! 0 |  198 | `    }else{` |
|  830187 |  199 | `      while( c>='0' && c<='9' ){` |
|     162 |  200 | `        width = width*10 + c - '0';` |
|     162 |  201 | `        c = *++zFormat;` |
|       2 |  202 | `      }` |
|       - |  203 | `    }` |
|  830027 |  204 | `    if( width > SXFMT_BUFSIZ-10 ){` |
|     ! 0 |  205 | `      width = SXFMT_BUFSIZ-10;` |
|     ! 0 |  206 | `    }` |
|       - |  207 | `    /* Get the precision */` |
|  830027 |  208 | `	precision = -1;` |
|  830027 |  209 | `    if( c=='.' ){` |
|  248083 |  210 | `      precision = 0;` |
|  248083 |  211 | `      c = *++zFormat;` |
|  248083 |  212 | `      if( c=='*' ){` |
|  247819 |  213 | `        precision = va_arg(ap,int);` |
|  247819 |  214 | `        if( precision<0 ) precision = -precision;` |
|  247819 |  215 | `        c = *++zFormat;` |
|  123912 |  216 | `      }else{` |
|     795 |  217 | `        while( c>='0' && c<='9' ){` |
|     531 |  218 | `          precision = precision*10 + c - '0';` |
|     531 |  219 | `          c = *++zFormat;` |
|       3 |  220 | `        }` |
|       - |  221 | `      }` |
|  124039 |  222 | `    }` |
|       - |  223 | `    /* Get the conversion type modifier */` |
|  830027 |  224 | `	flag_long = 0;` |
|  830027 |  225 | `    if( c=='l' \|\| c == 'q' /* BSD quad (expect a 64-bit integer) */ ){` |
|   54529 |  226 | `      flag_long = (c == 'q') ? 2 : 1;` |
|   54529 |  227 | `      c = *++zFormat;` |
|   54529 |  228 | `	  if( c == 'l' ){` |
|       - |  229 | `		  /* Standard printf emulation 'lld' (expect a 64bit integer) */` |
|     ! 0 |  230 | `		  flag_long = 2;` |
|     ! 0 |  231 | `	  }` |
|   27262 |  232 | `    }` |
|       - |  233 | `    /* Fetch the info entry for the field */` |
|  830027 |  234 | `    infop = 0;` |
|  830027 |  235 | `    xtype = SXFMT_ERROR;` |
| 3681509 |  236 | `	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
| 3681509 |  237 | `      if( c==aFmt[idx].fmttype ){` |
|  830027 |  238 | `        infop = &aFmt[idx];` |
|  830027 |  239 | `		xtype = infop->type;` |
|  830027 |  240 | `        break;` |
|       - |  241 | `      }` |
| 1425746 |  242 | `    }` |
|       - |  243 | `    /* zExtra is not used in this code path. */` |
|       - |  244 |  |
|       - |  245 | `    /*` |
|       - |  246 | `    ** At this point, variables are initialized as follows:` |
|       - |  247 | `    **` |
|       - |  248 | `    **   flag_alternateform          TRUE if a '#' is present.` |
|       - |  249 | `    **   flag_plussign               TRUE if a '+' is present.` |
|       - |  250 | `    **   flag_leftjustify            TRUE if a '-' is present or if the` |
|       - |  251 | `    **                               field width was negative.` |
|       - |  252 | `    **   flag_zeropad                TRUE if the width began with 0.` |
|       - |  253 | `    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed` |
|       - |  254 | `    **                               the conversion character.` |
|       - |  255 | `    **   flag_blanksign              TRUE if a ' ' is present.` |
|       - |  256 | `    **   width                       The specified field width.This is` |
|       - |  257 | `    **                               always non-negative.Zero is the default.` |
|       - |  258 | `    **   precision                   The specified precision.The default` |
|       - |  259 | `    **                               is -1.` |
|       - |  260 | `    **   xtype                       The class of the conversion.` |
|       - |  261 | `    **   infop                       Pointer to the appropriate info struct.` |
|       - |  262 | `    */` |
|  830027 |  263 | `    switch( xtype ){` |
|   28295 |  264 | `      case SXFMT_RADIX:` |
|   56595 |  265 | `        if( flag_long > 0 ){` |
|   54529 |  266 | `			if( flag_long > 1 ){` |
|       - |  267 | `				/* BSD quad: expect a 64-bit integer */` |
|   54517 |  268 | `				longvalue = va_arg(ap,sxi64);` |
|   27261 |  269 | `			}else{` |
|      13 |  270 | `				longvalue = va_arg(ap,sxlong);` |
|       - |  271 | `			}` |
|   27267 |  272 | `		}else{` |
|    2071 |  273 | `			if( infop->flags & SXFLAG_SIGNED ){` |
|    1069 |  274 | `				longvalue = va_arg(ap,sxi32);` |
|     537 |  275 | `			}else{` |
|    1007 |  276 | `				longvalue = va_arg(ap,sxu32);` |
|       - |  277 | `			}` |
|       - |  278 | `		}` |
|       - |  279 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|   56595 |  280 | `      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|       - |  281 | `#if 1` |
|       - |  282 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|       - |  283 | `        ** I think this is stupid.*/` |
|   56595 |  284 | `        if( longvalue==0 ) flag_alternateform = 0;` |
|       - |  285 | `#else` |
|       - |  286 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|       - |  287 | `        ** but leave the prefix for hex.*/` |
|       - |  288 | `        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;` |
|       - |  289 | `#endif` |
|   56595 |  290 | `        if( infop->flags & SXFLAG_SIGNED ){` |
|   55583 |  291 | `          if( longvalue<0 ){` |
|      96 |  292 | `            longvalue = -longvalue;` |
|       - |  293 | `			/* Ticket 1433-003 */` |
|      96 |  294 | `			if( longvalue < 0 ){` |
|       - |  295 | `				/* Overflow */` |
|     ! 0 |  296 | `				longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  297 | `			}` |
|      96 |  298 | `            prefix = '-';` |
|   55536 |  299 | `          }else if( flag_plussign )  prefix = '+';` |
|   55487 |  300 | `          else if( flag_blanksign )  prefix = ' ';` |
|   55487 |  301 | `          else                       prefix = 0;` |
|   27794 |  302 | `        }else{` |
|    1017 |  303 | `			if( longvalue<0 ){` |
|     ! 0 |  304 | `				longvalue = -longvalue;` |
|       - |  305 | `				/* Ticket 1433-003 */` |
|     ! 0 |  306 | `				if( longvalue < 0 ){` |
|       - |  307 | `					/* Overflow */` |
|     ! 0 |  308 | `					longvalue= 0x7FFFFFFFFFFFFFFF;` |
|     ! 0 |  309 | `				}` |
|     ! 0 |  310 | `			}` |
|    1017 |  311 | `			prefix = 0;` |
|       - |  312 | `		}` |
|   56595 |  313 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|     101 |  314 | `          precision = width-(prefix!=0);` |
|      50 |  315 | `        }` |
|   56595 |  316 | `        bufpt = &buf[SXFMT_BUFSIZ-1];` |
|       - |  317 | `        {` |
|       - |  318 | `          register char *cset;      /* Use registers for speed */` |
|       - |  319 | `          register int base;` |
|   56595 |  320 | `          cset = infop->charset;` |
|   56595 |  321 | `          base = infop->base;` |
|   28295 |  322 | `          do{                                           /* Convert to ascii */` |
|  173948 |  323 | `            *(--bufpt) = cset[longvalue%base];` |
|  173948 |  324 | `            longvalue = longvalue/base;` |
|  173948 |  325 | `          }while( longvalue>0 );` |
|       - |  326 | `        }` |
|   56595 |  327 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   56641 |  328 | `        for(idx=precision-length; idx>0; idx--){` |
|      47 |  329 | `          *(--bufpt) = '0';                             /* Zero pad */` |
|      24 |  330 | `        }` |
|   56595 |  331 | `        if( prefix ) *(--bufpt) = prefix;               /* Add sign */` |
|   56595 |  332 | `        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */` |
|       - |  333 | `          char *pre, x;` |
|       5 |  334 | `          pre = infop->prefix;` |
|       5 |  335 | `          if( *bufpt!=pre[0] ){` |
|      13 |  336 | `            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;` |
|       2 |  337 | `          }` |
|       2 |  338 | `        }` |
|   56595 |  339 | `        length = (int)(&buf[SXFMT_BUFSIZ-1]-bufpt);` |
|   56595 |  340 | `        break;` |
|     138 |  341 | `      case SXFMT_FLOAT:` |
|       - |  342 | `      case SXFMT_EXP:` |
|       - |  343 | `      case SXFMT_GENERIC:` |
|       - |  344 | `#ifndef SX_OMIT_FLOATINGPOINT` |
|     279 |  345 | `		realvalue = va_arg(ap,double);` |
|       - |  346 | `        /* handle NaN/Infinity specially before any arithmetic */` |
|     279 |  347 | `        if( PH7_IS_NAN(realvalue) ){` |
|       - |  348 | `            /* lowercase nan consistent with libc */` |
|     ! 0 |  349 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|       - |  350 | `            /* the value has no sign; make sure prefix is clear */` |
|     ! 0 |  351 | `            prefix = 0;` |
|     ! 0 |  352 | `            bufpt = buf + 3;` |
|     ! 0 |  353 | `            goto float_done;` |
|       - |  354 | `        }` |
|     279 |  355 | `        if( PH7_IS_INF(realvalue) ){` |
|     ! 0 |  356 | `            if( realvalue < 0.0 ){` |
|       - |  357 | `                /* negative infinity should be signed via prefix */` |
|     ! 0 |  358 | `                prefix = '-';` |
|     ! 0 |  359 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  360 | `                bufpt = buf + 3;` |
|     ! 0 |  361 | `            }else{` |
|       - |  362 | `                /* positive infinity treated like a plain value */` |
|     ! 0 |  363 | `                prefix = 0;` |
|     ! 0 |  364 | `                buf[0] = 'i'; buf[1] = 'n'; buf[2] = 'f';` |
|     ! 0 |  365 | `                bufpt = buf + 3;` |
|       - |  366 | `            }` |
|     ! 0 |  367 | `            goto float_done;` |
|       - |  368 | `        }` |
|     279 |  369 | `        if( precision<0 ) precision = 6;         /* Set default precision */` |
|     279 |  370 | `        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;` |
|     279 |  371 | `        if( realvalue<0.0 ){` |
|      25 |  372 | `          realvalue = -realvalue;` |
|      25 |  373 | `          prefix = '-';` |
|      13 |  374 | `        }else{` |
|     255 |  375 | `          if( flag_plussign )          prefix = '+';` |
|     255 |  376 | `          else if( flag_blanksign )    prefix = ' ';` |
|     255 |  377 | `          else                         prefix = 0;` |
|       - |  378 | `        }` |
|     279 |  379 | `        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;` |
|     279 |  380 | `        rounder = 0.0;` |
|       - |  381 | `#if 0` |
|       - |  382 | `        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */` |
|       - |  383 | `        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);` |
|       - |  384 | `#else` |
|       - |  385 | `        /* It makes more sense to use 0.5 */` |
|    4085 |  386 | `        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);` |
|       - |  387 | `#endif` |
|     279 |  388 | `        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;` |
|       - |  389 | `        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */` |
|     279 |  390 | `        exp = 0;` |
|     279 |  391 | `        if( realvalue>0.0 ){` |
|     263 |  392 | `          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }` |
|     367 |  393 | `          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }` |
|     253 |  394 | `          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }` |
|     307 |  395 | `          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }` |
|     253 |  396 | `          if( exp>350 \|\| exp<-350 ){` |
|     ! 0 |  397 | `            buf[0] = 'n'; buf[1] = 'a'; buf[2] = 'n';` |
|     ! 0 |  398 | `            bufpt = buf + 3;` |
|     ! 0 |  399 | `            goto float_done;` |
|       - |  400 | `          }` |
|     125 |  401 | `        }` |
|     279 |  402 | `        bufpt = buf;` |
|       - |  403 | `        /*` |
|       - |  404 | `        ** If the field type is etGENERIC, then convert to either etEXP` |
|       - |  405 | `        ** or etFLOAT, as appropriate.` |
|       - |  406 | `        */` |
|     279 |  407 | `        flag_exp = xtype==SXFMT_EXP;` |
|     279 |  408 | `        if( xtype!=SXFMT_FLOAT ){` |
|     277 |  409 | `          realvalue += rounder;` |
|     277 |  410 | `          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }` |
|     137 |  411 | `        }` |
|     279 |  412 | `        if( xtype==SXFMT_GENERIC ){` |
|     277 |  413 | `          flag_rtz = !flag_alternateform;` |
|     277 |  414 | `          if( exp<-4 \|\| exp>precision ){` |
|     ! 0 |  415 | `            xtype = SXFMT_EXP;` |
|     ! 0 |  416 | `          }else{` |
|     277 |  417 | `            precision = precision - exp;` |
|     277 |  418 | `            xtype = SXFMT_FLOAT;` |
|       - |  419 | `          }` |
|     140 |  420 | `        }else{` |
|       3 |  421 | `          flag_rtz = 0;` |
|       - |  422 | `        }` |
|       - |  423 | `        /*` |
|       - |  424 | `        ** The "exp+precision" test causes output to be of type etEXP if` |
|       - |  425 | `        ** the precision is too large to fit in buf[].` |
|       - |  426 | `        */` |
|     279 |  427 | `        nsd = 0;` |
|     417 |  428 | `        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){` |
|     279 |  429 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     279 |  430 | `          if( prefix ) *(bufpt++) = prefix;         /* Sign */` |
|     279 |  431 | `          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */` |
|     673 |  432 | `          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     279 |  433 | `          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */` |
|     295 |  434 | `          for(exp++; exp<0 && precision>0; precision--, exp++){` |
|      17 |  435 | `            *(bufpt++) = '0';` |
|       9 |  436 | `          }` |
|    3929 |  437 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     279 |  438 | `          *(bufpt--) = 0;                           /* Null terminate */` |
|     279 |  439 | `          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */` |
|    2949 |  440 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     277 |  441 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     137 |  442 | `          }` |
|     279 |  443 | `          bufpt++;                            /* point to next free slot */` |
|     141 |  444 | `        }else{    /* etEXP or etGENERIC */` |
|     ! 0 |  445 | `          flag_dp = (precision>0 \|\| flag_alternateform);` |
|     ! 0 |  446 | `          if( prefix ) *(bufpt++) = prefix;   /* Sign */` |
|     ! 0 |  447 | `          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */` |
|     ! 0 |  448 | `          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */` |
|     ! 0 |  449 | `          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);` |
|     ! 0 |  450 | `          bufpt--;                            /* point to last digit */` |
|     ! 0 |  451 | `          if( flag_rtz && flag_dp ){          /* Remove tail zeros */` |
|     ! 0 |  452 | `            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;` |
|     ! 0 |  453 | `            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;` |
|     ! 0 |  454 | `          }` |
|     ! 0 |  455 | `          bufpt++;                            /* point to next free slot */` |
|     ! 0 |  456 | `          if( exp \|\| flag_exp ){` |
|     ! 0 |  457 | `            *(bufpt++) = infop->charset[0];` |
|     ! 0 |  458 | `            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */` |
|     ! 0 |  459 | `            else       { *(bufpt++) = '+'; }` |
|     ! 0 |  460 | `            if( exp>=100 ){` |
|     ! 0 |  461 | `              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */` |
|     ! 0 |  462 | `              exp %= 100;` |
|     ! 0 |  463 | `            }` |
|     ! 0 |  464 | `            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */` |
|     ! 0 |  465 | `            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */` |
|     ! 0 |  466 | `          }` |
|       - |  467 | `        }` |
|     ! 0 |  468 | `        float_done:` |
|       - |  469 | `        /* The converted number is in buf[] and zero terminated.Output it.` |
|       - |  470 | `        ** Note that the number is in the usual order, not reversed as with` |
|       - |  471 | `        ** integer conversions.*/` |
|     279 |  472 | `        length = (int)(bufpt-buf);` |
|     279 |  473 | `        bufpt = buf;` |
|       - |  474 |  |
|       - |  475 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|       - |  476 | `        ** set and we are not left justified */` |
|     279 |  477 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|       - |  478 | `          int i;` |
|     ! 0 |  479 | `          int nPad = width - length;` |
|     ! 0 |  480 | `          for(i=width; i>=nPad; i--){` |
|     ! 0 |  481 | `            bufpt[i] = bufpt[i-nPad];` |
|     ! 0 |  482 | `          }` |
|     ! 0 |  483 | `          i = prefix!=0;` |
|     ! 0 |  484 | `          while( nPad-- ) bufpt[i++] = '0';` |
|     ! 0 |  485 | `          length = width;` |
|     ! 0 |  486 | `        }` |
|       - |  487 | `#else` |
|       - |  488 | `         bufpt = " ";` |
|       - |  489 | `		 length = (int)sizeof(" ") - 1;` |
|       - |  490 | `#endif /* SX_OMIT_FLOATINGPOINT */` |
|     279 |  491 | `        break;` |
|     ! 0 |  492 | `      case SXFMT_SIZE:{` |
|     ! 0 |  493 | `		 int *pSize = va_arg(ap,int *);` |
|     ! 0 |  494 | `		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;` |
|     ! 0 |  495 | `		 length = width = 0;` |
|       - |  496 | `					  }` |
|     ! 0 |  497 | `        break;` |
|       1 |  498 | `      case SXFMT_PERCENT:` |
|       3 |  499 | `        buf[0] = '%';` |
|       3 |  500 | `        bufpt = buf;` |
|       3 |  501 | `        length = 1;` |
|       3 |  502 | `        break;` |
|    4905 |  503 | `      case SXFMT_CHARX:` |
|    9814 |  504 | `        c = va_arg(ap,int);` |
|    9814 |  505 | `		buf[0] = (char)c;` |
|       - |  506 | `		/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    9814 |  507 | `		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;` |
|    9814 |  508 | `        if( precision>=0 ){` |
|     ! 0 |  509 | `          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;` |
|     ! 0 |  510 | `          length = precision;` |
|     ! 0 |  511 | `        }else{` |
|    9814 |  512 | `          length =1;` |
|       - |  513 | `        }` |
|    9814 |  514 | `        bufpt = buf;` |
|    9814 |  515 | `        break;` |
|  124517 |  516 | `      case SXFMT_STRING:` |
|  249039 |  517 | `        bufpt = va_arg(ap,char*);` |
|  249039 |  518 | `        if( bufpt==0 ){` |
|     ! 0 |  519 | `          bufpt = " ";` |
|     ! 0 |  520 | `		  length = (int)sizeof(" ")-1;` |
|     ! 0 |  521 | `		  break;` |
|       - |  522 | `        }` |
|  249039 |  523 | `		length = precision;` |
|  249039 |  524 | `		if( precision < 0 ){` |
|       - |  525 | `			/* Symisc extension */` |
|    1227 |  526 | `			length = (int)SyStrlen(bufpt);` |
|     611 |  527 | `		}` |
|  249039 |  528 | `        if( precision>=0 && precision<length ) length = precision;` |
|  249039 |  529 | `        break;` |
|  257155 |  530 | `	case SXFMT_RAWSTR:{` |
|       - |  531 | `		/* Symisc extension */` |
|  514315 |  532 | `		SyString *pStr = va_arg(ap,SyString *);` |
|  514315 |  533 | `		if( pStr == 0 \|\| pStr->zString == 0 ){` |
|     ! 0 |  534 | `			 bufpt = " ";` |
|     ! 0 |  535 | `		     length = (int)sizeof(char);` |
|     ! 0 |  536 | `		     break;` |
|       - |  537 | `		}` |
|  514315 |  538 | `		bufpt = (char *)pStr->zString;` |
|  514315 |  539 | `		length = (int)pStr->nByte;` |
|  514315 |  540 | `		break;` |
|       - |  541 | `					  }` |
|     ! 0 |  542 | `      case SXFMT_ERROR:` |
|     ! 0 |  543 | `        buf[0] = '?';` |
|     ! 0 |  544 | `        bufpt = buf;` |
|     ! 0 |  545 | `		length = (int)sizeof(char);` |
|     ! 0 |  546 | `        if( c==0 ) zFormat--;` |
|     ! 0 |  547 | `        break;` |
|       - |  548 | `    }/* End switch over the format type */` |
|       - |  549 | `    /*` |
|       - |  550 | `    ** The text of the conversion is pointed to by "bufpt" and is` |
|       - |  551 | `    ** "length" characters long.The field width is "width".Do` |
|       - |  552 | `    ** the output.` |
|       - |  553 | `    */` |
|  830027 |  554 | `    if( !flag_leftjustify ){` |
|       - |  555 | `      register int nspace;` |
|  830027 |  556 | `      nspace = width-length;` |
|  830027 |  557 | `      if( nspace>0 ){` |
|      37 |  558 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  559 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  560 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  561 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  562 | `			}` |
|     ! 0 |  563 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  564 | `        }` |
|      37 |  565 | `        if( nspace>0 ){` |
|      37 |  566 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|      37 |  567 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  568 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  569 | `			}` |
|      18 |  570 | `		}` |
|      18 |  571 | `      }` |
|  415011 |  572 | `    }` |
|  830027 |  573 | `    if( length>0 ){` |
|  830003 |  574 | `		rc = xConsumer(bufpt,(unsigned int)length,pUserData);` |
|  830003 |  575 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  576 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  577 | `		}` |
|  414999 |  578 | `    }` |
|  830027 |  579 | `    if( flag_leftjustify ){` |
|       - |  580 | `      register int nspace;` |
|     ! 0 |  581 | `      nspace = width-length;` |
|     ! 0 |  582 | `      if( nspace>0 ){` |
|     ! 0 |  583 | `        while( nspace>=etSPACESIZE ){` |
|     ! 0 |  584 | `			rc = xConsumer(spaces,etSPACESIZE,pUserData);` |
|     ! 0 |  585 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  586 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  587 | `			}` |
|     ! 0 |  588 | `			nspace -= etSPACESIZE;` |
|     ! 0 |  589 | `        }` |
|     ! 0 |  590 | `        if( nspace>0 ){` |
|     ! 0 |  591 | `			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);` |
|     ! 0 |  592 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  593 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|       - |  594 | `			}` |
|     ! 0 |  595 | `		}` |
|     ! 0 |  596 | `      }` |
|     ! 0 |  597 | `    }` |
|  415016 |  598 | `  }/* End for loop over the format string */` |
|   65443 |  599 | `  return errorflag ? SXERR_FORMAT : SXRET_OK;` |
|  157802 |  600 |  |
|       - |  601 | `/* SPDX-SnippetEnd */` |
| 1825096 |  602 | `static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)` |
|       5 |  603 |  |
| 1825101 |  604 | `	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;` |
| 1825101 |  605 | `	sxi32 rc = SXERR_ABORT;` |
| 1825101 |  606 | `	switch(pConsumer->nType){` |
|      78 |  607 | `	case SXFMT_CONS_PROC:` |
|       - |  608 | `			/* User callback */` |
|     157 |  609 | `			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);` |
|     157 |  610 | `			break;` |
|  912470 |  611 | `	case SXFMT_CONS_BLOB:` |
|       - |  612 | `			/* Blob consumer */` |
| 1824945 |  613 | `			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);` |
| 1824940 |  614 | `			break;` |
|     ! 0 |  615 | `		default:` |
|       - |  616 | `			/* Unknown consumer */` |
|     ! 0 |  617 | `			break;` |
|       - |  618 | `	}` |
|       - |  619 | `	/* Update total number of bytes consumed so far */` |
| 1825101 |  620 | `	pConsumer->nLen += nLen;` |
| 1825101 |  621 | `	pConsumer->rc = rc;` |
| 1825101 |  622 | `	return rc;` |
|       5 |  623 |  |
|  315594 |  624 | `static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)` |
|       5 |  625 |  |
|       - |  626 | `	SyFmtConsumer sCons;` |
|  315599 |  627 | `	sCons.nType = nType;` |
|  315599 |  628 | `	sCons.rc = SXRET_OK;` |
|  315599 |  629 | `	sCons.nLen = 0;` |
|  315599 |  630 | `	if( pOutLen ){` |
|   67807 |  631 | `		*pOutLen = 0;` |
|   33901 |  632 | `	}` |
|  315599 |  633 | `	switch(nType){` |
|       6 |  634 | `	case SXFMT_CONS_PROC:` |
|       - |  635 | `#if defined(UNTRUST)` |
|       - |  636 | `			if( xUserCons == 0 ){` |
|       - |  637 | `				return SXERR_EMPTY;` |
|       - |  638 | `			}` |
|       - |  639 | `#endif` |
|      13 |  640 | `			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;` |
|      13 |  641 | `			sCons.uConsumer.sFunc.pUserData	    = pUserData;` |
|      13 |  642 | `		break;` |
|  157791 |  643 | `		case SXFMT_CONS_BLOB:` |
|  315587 |  644 | `			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;` |
|  315587 |  645 | `			break;` |
|     ! 0 |  646 | `		default:` |
|     ! 0 |  647 | `			return SXERR_UNKNOWN;` |
|       - |  648 | `	}` |
|  315599 |  649 | `	InternFormat(FormatConsumer,&sCons,zFormat,ap);` |
|  315599 |  650 | `	if( pOutLen ){` |
|   67807 |  651 | `		*pOutLen = sCons.nLen;` |
|   33901 |  652 | `	}` |
|  315599 |  653 | `	return sCons.rc;` |
|  157802 |  654 |  |
|      12 |  655 | `PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)` |
|       1 |  656 |  |
|       - |  657 | `	va_list ap;` |
|       - |  658 | `	sxi32 rc;` |
|       - |  659 | `#if defined(UNTRUST)` |
|       - |  660 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  661 | `		return SXERR_EMPTY;` |
|       - |  662 | `	}` |
|       - |  663 | `#endif` |
|      13 |  664 | `	va_start(ap,zFormat);` |
|      13 |  665 | `	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);` |
|      13 |  666 | `	va_end(ap);` |
|      13 |  667 | `	return rc;` |
|       1 |  668 |  |
|   66278 |  669 | `PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)` |
|       5 |  670 |  |
|       - |  671 | `	va_list ap;` |
|       - |  672 | `	sxu32 n;` |
|       - |  673 | `#if defined(UNTRUST)` |
|       - |  674 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  675 | `		return 0;` |
|       - |  676 | `	}` |
|       - |  677 | `#endif` |
|   66283 |  678 | `	va_start(ap,zFormat);` |
|   66283 |  679 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|   66283 |  680 | `	va_end(ap);` |
|   66283 |  681 | `	return n;` |
|       5 |  682 |  |
|    1524 |  683 | `PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)` |
|       5 |  684 |  |
|    1529 |  685 | `	sxu32 n = 0; /* cc warning */` |
|       - |  686 | `#if defined(UNTRUST)` |
|       - |  687 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  688 | `		return 0;` |
|       - |  689 | `	}` |
|       - |  690 | `#endif` |
|    1529 |  691 | `	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);` |
|    1529 |  692 | `	return n;` |
|       5 |  693 |  |
|  247780 |  694 | `PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)` |
|       5 |  695 |  |
|       - |  696 | `	SyBlob sBlob;` |
|       - |  697 | `	va_list ap;` |
|       - |  698 | `	sxu32 n;` |
|       - |  699 | `#if defined(UNTRUST)` |
|       - |  700 | `	if( SX_EMPTY_STR(zFormat) ){` |
|       - |  701 | `		return 0;` |
|       - |  702 | `	}` |
|       - |  703 | `#endif` |
|  247785 |  704 | `	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){` |
|     ! 0 |  705 | `		return 0;` |
|       - |  706 | `	}` |
|  247785 |  707 | `	va_start(ap,zFormat);` |
|  247785 |  708 | `	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);` |
|  247785 |  709 | `	va_end(ap);` |
|  247785 |  710 | `	n = SyBlobLength(&sBlob);` |
|       - |  711 | `	/* Append the null terminator */` |
|  247785 |  712 | `	sBlob.mByte++;` |
|  247785 |  713 | `	SyBlobAppend(&sBlob,"\0",sizeof(char));` |
|  247785 |  714 | `	return n;` |
|  123895 |  715 |  |
|       - |  716 |  |
