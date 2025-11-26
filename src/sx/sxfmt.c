/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxproto.h"

#define SXFMT_BUFSIZ 1024 /* Conversion buffer size */
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define SXFMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define SXFMT_FLOAT       2 /* Floating point.%f */
#define SXFMT_EXP         3 /* Exponentional notation.%e and %E */
#define SXFMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define SXFMT_SIZE        5 /* Total number of characters processed so far.%n */
#define SXFMT_STRING      6 /* Strings.%s */
#define SXFMT_PERCENT     7 /* Percent symbol.%% */
#define SXFMT_CHARX       8 /* Characters.%c */
#define SXFMT_ERROR       9 /* Used to indicate no such conversion type */
/* Extension by Symisc Systems */
#define SXFMT_RAWSTR     13 /* %z Pointer to raw string (SyString *) */
#define SXFMT_UNUSED     15
/*
** Allowed values for SyFmtInfo.flags
*/
#define SXFLAG_SIGNED	0x01
#define SXFLAG_UNSIGNED 0x02
/* Allowed values for SyFmtConsumer.nType */
#define SXFMT_CONS_PROC		1	/* Consumer is a procedure */
#define SXFMT_CONS_STR		2	/* Consumer is a managed string */
#define SXFMT_CONS_FILE		5	/* Consumer is an open File */
#define SXFMT_CONS_BLOB		6	/* Consumer is a BLOB */
/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct SyFmtInfo SyFmtInfo;
struct SyFmtInfo
{
  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */
  sxu8 base;     /* The base for radix conversion */
  int flags;    /* One or more of SXFLAG_ constants below */
  sxu8 type;     /* Conversion paradigm */
  char *charset; /* The character set for conversion */
  char *prefix;  /* Prefix on non-zero values in alt format */
};
typedef struct SyFmtConsumer SyFmtConsumer;
struct SyFmtConsumer
{
	sxu32 nLen; /* Total output length */
	sxi32 nType; /* Type of the consumer see below */
	sxi32 rc;	/* Consumer return value;Abort processing if rc != SXRET_OK */
 union{
	struct{
	ProcConsumer xUserConsumer;
	void *pUserData;
	}sFunc;
	SyBlob *pBlob;
 }uConsumer;
};
#ifndef SX_OMIT_FLOATINGPOINT
static int getdigit(sxlongreal *val,int *cnt)
{
  sxlongreal d;
  int digit;

  if( (*cnt)++ >= 16 ){
	  return '0';
  }
  digit = (int)*val;
  d = digit;
   *val = (*val - d)*10.0;
  return digit + '0' ;
}
#endif /* SX_OMIT_FLOATINGPOINT */
/*
 * The following routine was taken from the SQLITE2 source tree and was
 * extended by Symisc Systems to fit its need.
 * Status: Public Domain
 */
static sxi32 InternFormat(ProcConsumer xConsumer,void *pUserData,const char *zFormat,va_list ap)
{
	/*
	 * The following table is searched linearly, so it is good to put the most frequently
	 * used conversion types first.
	 */
static const SyFmtInfo aFmt[] = {
  {  'd', 10, SXFLAG_SIGNED, SXFMT_RADIX, "0123456789",0    },
  {  's',  0, 0, SXFMT_STRING,     0,                  0    },
  {  'c',  0, 0, SXFMT_CHARX,      0,                  0    },
  {  'x', 16, 0, SXFMT_RADIX,      "0123456789abcdef", "x0" },
  {  'X', 16, 0, SXFMT_RADIX,      "0123456789ABCDEF", "X0" },
         /* -- Extensions by Symisc Systems -- */
  {  'z',  0, 0, SXFMT_RAWSTR,     0,                   0   }, /* Pointer to a raw string (SyString *) */
  {  'B',  2, 0, SXFMT_RADIX,      "01",                "b0"},
         /* -- End of Extensions -- */
  {  'o',  8, 0, SXFMT_RADIX,      "01234567",         "0"  },
  {  'u', 10, 0, SXFMT_RADIX,      "0123456789",       0    },
#ifndef SX_OMIT_FLOATINGPOINT
  {  'f',  0, SXFLAG_SIGNED, SXFMT_FLOAT,       0,     0    },
  {  'e',  0, SXFLAG_SIGNED, SXFMT_EXP,        "e",    0    },
  {  'E',  0, SXFLAG_SIGNED, SXFMT_EXP,        "E",    0    },
  {  'g',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "e",    0    },
  {  'G',  0, SXFLAG_SIGNED, SXFMT_GENERIC,    "E",    0    },
#endif
  {  'i', 10, SXFLAG_SIGNED, SXFMT_RADIX,"0123456789", 0    },
  {  'n',  0, 0, SXFMT_SIZE,       0,                  0    },
  {  '%',  0, 0, SXFMT_PERCENT,    0,                  0    },
  {  'p', 10, 0, SXFMT_RADIX,      "0123456789",       0    }
};
  int c;                     /* Next character in the format string */
  char *bufpt;               /* Pointer to the conversion buffer */
  int precision;             /* Precision of the current field */
  int length;                /* Length of the field */
  int idx;                   /* A general purpose loop counter */
  int width;                 /* Width of the current field */
  sxu8 flag_leftjustify;   /* True if "-" flag is present */
  sxu8 flag_plussign;      /* True if "+" flag is present */
  sxu8 flag_blanksign;     /* True if " " flag is present */
  sxu8 flag_alternateform; /* True if "#" flag is present */
  sxu8 flag_zeropad;       /* True if field width constant starts with zero */
  sxu8 flag_long;          /* True if "l" flag is present */
  sxi64 longvalue;         /* Value for integer types */
  const SyFmtInfo *infop;  /* Pointer to the appropriate info structure */
  char buf[SXFMT_BUFSIZ];  /* Conversion buffer */
  char prefix;             /* Prefix character."+" or "-" or " " or '\0'.*/
  sxu8 errorflag = 0;      /* True if an error is encountered */
  sxu8 xtype;              /* Conversion paradigm */
  char *zExtra;
  static char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
#ifndef SX_OMIT_FLOATINGPOINT
  sxlongreal realvalue;    /* Value for real types */
  int  exp;                /* exponent of real numbers */
  double rounder;          /* Used for rounding floating point values */
  sxu8 flag_dp;            /* True if decimal point should be shown */
  sxu8 flag_rtz;           /* True if trailing zeros should be removed */
  sxu8 flag_exp;           /* True to force display of the exponent */
  int nsd;                 /* Number of significant digits returned */
#endif
  int rc;

  length = 0;
  bufpt = 0;
  for(; (c=(*zFormat))!=0; ++zFormat){
    if( c!='%' ){
      unsigned int amt;
      bufpt = (char *)zFormat;
      amt = 1;
      while( (c=(*++zFormat))!='%' && c!=0 ) amt++;
	  rc = xConsumer((const void *)bufpt,amt,pUserData);
	  if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
	  }
      if( c==0 ){
		  return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;
	  }
    }
    if( (c=(*++zFormat))==0 ){
      errorflag = 1;
	  rc = xConsumer("%",sizeof("%")-1,pUserData);
	  if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
	  }
      return errorflag > 0 ? SXERR_FORMAT : SXRET_OK;
    }
    /* Find out what flags are present */
    flag_leftjustify = flag_plussign = flag_blanksign =
     flag_alternateform = flag_zeropad = 0;
    do{
      switch( c ){
        case '-':   flag_leftjustify = 1;     c = 0;   break;
        case '+':   flag_plussign = 1;        c = 0;   break;
        case ' ':   flag_blanksign = 1;       c = 0;   break;
        case '#':   flag_alternateform = 1;   c = 0;   break;
        case '0':   flag_zeropad = 1;         c = 0;   break;
        default:                                       break;
      }
    }while( c==0 && (c=(*++zFormat))!=0 );
    /* Get the field width */
    width = 0;
    if( c=='*' ){
      width = va_arg(ap,int);
      if( width<0 ){
        flag_leftjustify = 1;
        width = -width;
      }
      c = *++zFormat;
    }else{
      while( c>='0' && c<='9' ){
        width = width*10 + c - '0';
        c = *++zFormat;
      }
    }
    if( width > SXFMT_BUFSIZ-10 ){
      width = SXFMT_BUFSIZ-10;
    }
    /* Get the precision */
	precision = -1;
    if( c=='.' ){
      precision = 0;
      c = *++zFormat;
      if( c=='*' ){
        precision = va_arg(ap,int);
        if( precision<0 ) precision = -precision;
        c = *++zFormat;
      }else{
        while( c>='0' && c<='9' ){
          precision = precision*10 + c - '0';
          c = *++zFormat;
        }
      }
    }
    /* Get the conversion type modifier */
	flag_long = 0;
    if( c=='l' || c == 'q' /* BSD quad (expect a 64-bit integer) */ ){
      flag_long = (c == 'q') ? 2 : 1;
      c = *++zFormat;
	  if( c == 'l' ){
		  /* Standard printf emulation 'lld' (expect a 64bit integer) */
		  flag_long = 2;
	  }
    }
    /* Fetch the info entry for the field */
    infop = 0;
    xtype = SXFMT_ERROR;
	for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){
      if( c==aFmt[idx].fmttype ){
        infop = &aFmt[idx];
		xtype = infop->type;
        break;
      }
    }
    zExtra = 0;

    /*
    ** At this point, variables are initialized as follows:
    **
    **   flag_alternateform          TRUE if a '#' is present.
    **   flag_plussign               TRUE if a '+' is present.
    **   flag_leftjustify            TRUE if a '-' is present or if the
    **                               field width was negative.
    **   flag_zeropad                TRUE if the width began with 0.
    **   flag_long                   TRUE if the letter 'l' (ell) or 'q'(BSD quad) prefixed
    **                               the conversion character.
    **   flag_blanksign              TRUE if a ' ' is present.
    **   width                       The specified field width.This is
    **                               always non-negative.Zero is the default.
    **   precision                   The specified precision.The default
    **                               is -1.
    **   xtype                       The class of the conversion.
    **   infop                       Pointer to the appropriate info struct.
    */
    switch( xtype ){
      case SXFMT_RADIX:
        if( flag_long > 0 ){
			if( flag_long > 1 ){
				/* BSD quad: expect a 64-bit integer */
				longvalue = va_arg(ap,sxi64);
			}else{
				longvalue = va_arg(ap,sxlong);
			}
		}else{
			if( infop->flags & SXFLAG_SIGNED ){
				longvalue = va_arg(ap,sxi32);
			}else{
				longvalue = va_arg(ap,sxu32);
			}
		}
		/* Limit the precision to prevent overflowing buf[] during conversion */
      if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;
#if 1
        /* For the format %#x, the value zero is printed "0" not "0x0".
        ** I think this is stupid.*/
        if( longvalue==0 ) flag_alternateform = 0;
#else
        /* More sensible: turn off the prefix for octal (to prevent "00"),
        ** but leave the prefix for hex.*/
        if( longvalue==0 && infop->base==8 ) flag_alternateform = 0;
#endif
        if( infop->flags & SXFLAG_SIGNED ){
          if( longvalue<0 ){
            longvalue = -longvalue;
			/* Ticket 1433-003 */
			if( longvalue < 0 ){
				/* Overflow */
				longvalue= 0x7FFFFFFFFFFFFFFF;
			}
            prefix = '-';
          }else if( flag_plussign )  prefix = '+';
          else if( flag_blanksign )  prefix = ' ';
          else                       prefix = 0;
        }else{
			if( longvalue<0 ){
				longvalue = -longvalue;
				/* Ticket 1433-003 */
				if( longvalue < 0 ){
					/* Overflow */
					longvalue= 0x7FFFFFFFFFFFFFFF;
				}
			}
			prefix = 0;
		}
        if( flag_zeropad && precision<width-(prefix!=0) ){
          precision = width-(prefix!=0);
        }
        bufpt = &buf[SXFMT_BUFSIZ-1];
        {
          register char *cset;      /* Use registers for speed */
          register int base;
          cset = infop->charset;
          base = infop->base;
          do{                                           /* Convert to ascii */
            *(--bufpt) = cset[longvalue%base];
            longvalue = longvalue/base;
          }while( longvalue>0 );
        }
        length = &buf[SXFMT_BUFSIZ-1]-bufpt;
        for(idx=precision-length; idx>0; idx--){
          *(--bufpt) = '0';                             /* Zero pad */
        }
        if( prefix ) *(--bufpt) = prefix;               /* Add sign */
        if( flag_alternateform && infop->prefix ){      /* Add "0" or "0x" */
          char *pre, x;
          pre = infop->prefix;
          if( *bufpt!=pre[0] ){
            for(pre=infop->prefix; (x=(*pre))!=0; pre++) *(--bufpt) = x;
          }
        }
        length = &buf[SXFMT_BUFSIZ-1]-bufpt;
        break;
      case SXFMT_FLOAT:
      case SXFMT_EXP:
      case SXFMT_GENERIC:
#ifndef SX_OMIT_FLOATINGPOINT
		realvalue = va_arg(ap,double);
        if( precision<0 ) precision = 6;         /* Set default precision */
        if( precision>SXFMT_BUFSIZ-40) precision = SXFMT_BUFSIZ-40;
        if( realvalue<0.0 ){
          realvalue = -realvalue;
          prefix = '-';
        }else{
          if( flag_plussign )          prefix = '+';
          else if( flag_blanksign )    prefix = ' ';
          else                         prefix = 0;
        }
        if( infop->type==SXFMT_GENERIC && precision>0 ) precision--;
        rounder = 0.0;
#if 0
        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */
        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);
#else
        /* It makes more sense to use 0.5 */
        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);
#endif
        if( infop->type==SXFMT_FLOAT ) realvalue += rounder;
        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
        exp = 0;
        if( realvalue>0.0 ){
          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }
          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }
          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }
          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }
          if( exp>350 || exp<-350 ){
            bufpt = "NaN";
            length = 3;
            break;
          }
        }
        bufpt = buf;
        /*
        ** If the field type is etGENERIC, then convert to either etEXP
        ** or etFLOAT, as appropriate.
        */
        flag_exp = xtype==SXFMT_EXP;
        if( xtype!=SXFMT_FLOAT ){
          realvalue += rounder;
          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }
        }
        if( xtype==SXFMT_GENERIC ){
          flag_rtz = !flag_alternateform;
          if( exp<-4 || exp>precision ){
            xtype = SXFMT_EXP;
          }else{
            precision = precision - exp;
            xtype = SXFMT_FLOAT;
          }
        }else{
          flag_rtz = 0;
        }
        /*
        ** The "exp+precision" test causes output to be of type etEXP if
        ** the precision is too large to fit in buf[].
        */
        nsd = 0;
        if( xtype==SXFMT_FLOAT && exp+precision<SXFMT_BUFSIZ-30 ){
          flag_dp = (precision>0 || flag_alternateform);
          if( prefix ) *(bufpt++) = prefix;         /* Sign */
          if( exp<0 )  *(bufpt++) = '0';            /* Digits before "." */
          else for(; exp>=0; exp--) *(bufpt++) = (char)getdigit(&realvalue,&nsd);
          if( flag_dp ) *(bufpt++) = '.';           /* The decimal point */
          for(exp++; exp<0 && precision>0; precision--, exp++){
            *(bufpt++) = '0';
          }
          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);
          *(bufpt--) = 0;                           /* Null terminate */
          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */
            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;
            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;
          }
          bufpt++;                            /* point to next free slot */
        }else{    /* etEXP or etGENERIC */
          flag_dp = (precision>0 || flag_alternateform);
          if( prefix ) *(bufpt++) = prefix;   /* Sign */
          *(bufpt++) = (char)getdigit(&realvalue,&nsd);  /* First digit */
          if( flag_dp ) *(bufpt++) = '.';     /* Decimal point */
          while( (precision--)>0 ) *(bufpt++) = (char)getdigit(&realvalue,&nsd);
          bufpt--;                            /* point to last digit */
          if( flag_rtz && flag_dp ){          /* Remove tail zeros */
            while( bufpt>=buf && *bufpt=='0' ) *(bufpt--) = 0;
            if( bufpt>=buf && *bufpt=='.' ) *(bufpt--) = 0;
          }
          bufpt++;                            /* point to next free slot */
          if( exp || flag_exp ){
            *(bufpt++) = infop->charset[0];
            if( exp<0 ){ *(bufpt++) = '-'; exp = -exp; } /* sign of exp */
            else       { *(bufpt++) = '+'; }
            if( exp>=100 ){
              *(bufpt++) = (char)((exp/100)+'0');                /* 100's digit */
              exp %= 100;
            }
            *(bufpt++) = (char)(exp/10+'0');                     /* 10's digit */
            *(bufpt++) = (char)(exp%10+'0');                     /* 1's digit */
          }
        }
        /* The converted number is in buf[] and zero terminated.Output it.
        ** Note that the number is in the usual order, not reversed as with
        ** integer conversions.*/
        length = bufpt-buf;
        bufpt = buf;

        /* Special case:  Add leading zeros if the flag_zeropad flag is
        ** set and we are not left justified */
        if( flag_zeropad && !flag_leftjustify && length < width){
          int i;
          int nPad = width - length;
          for(i=width; i>=nPad; i--){
            bufpt[i] = bufpt[i-nPad];
          }
          i = prefix!=0;
          while( nPad-- ) bufpt[i++] = '0';
          length = width;
        }
#else
         bufpt = " ";
		 length = (int)sizeof(" ") - 1;
#endif /* SX_OMIT_FLOATINGPOINT */
        break;
      case SXFMT_SIZE:{
		 int *pSize = va_arg(ap,int *);
		 *pSize = ((SyFmtConsumer *)pUserData)->nLen;
		 length = width = 0;
					  }
        break;
      case SXFMT_PERCENT:
        buf[0] = '%';
        bufpt = buf;
        length = 1;
        break;
      case SXFMT_CHARX:
        c = va_arg(ap,int);
		buf[0] = (char)c;
		/* Limit the precision to prevent overflowing buf[] during conversion */
		if( precision>SXFMT_BUFSIZ-40 ) precision = SXFMT_BUFSIZ-40;
        if( precision>=0 ){
          for(idx=1; idx<precision; idx++) buf[idx] = (char)c;
          length = precision;
        }else{
          length =1;
        }
        bufpt = buf;
        break;
      case SXFMT_STRING:
        bufpt = va_arg(ap,char*);
        if( bufpt==0 ){
          bufpt = " ";
		  length = (int)sizeof(" ")-1;
		  break;
        }
		length = precision;
		if( precision < 0 ){
			/* Symisc extension */
			length = (int)SyStrlen(bufpt);
		}
        if( precision>=0 && precision<length ) length = precision;
        break;
	case SXFMT_RAWSTR:{
		/* Symisc extension */
		SyString *pStr = va_arg(ap,SyString *);
		if( pStr == 0 || pStr->zString == 0 ){
			 bufpt = " ";
		     length = (int)sizeof(char);
		     break;
		}
		bufpt = (char *)pStr->zString;
		length = (int)pStr->nByte;
		break;
					  }
      case SXFMT_ERROR:
        buf[0] = '?';
        bufpt = buf;
		length = (int)sizeof(char);
        if( c==0 ) zFormat--;
        break;
    }/* End switch over the format type */
    /*
    ** The text of the conversion is pointed to by "bufpt" and is
    ** "length" characters long.The field width is "width".Do
    ** the output.
    */
    if( !flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
    if( length>0 ){
		rc = xConsumer(bufpt,(unsigned int)length,pUserData);
		if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
		}
    }
    if( flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
  }/* End for loop over the format string */
  return errorflag ? SXERR_FORMAT : SXRET_OK;
}
static sxi32 FormatConsumer(const void *pSrc,unsigned int nLen,void *pData)
{
	SyFmtConsumer *pConsumer = (SyFmtConsumer *)pData;
	sxi32 rc = SXERR_ABORT;
	switch(pConsumer->nType){
	case SXFMT_CONS_PROC:
			/* User callback */
			rc = pConsumer->uConsumer.sFunc.xUserConsumer(pSrc,nLen,pConsumer->uConsumer.sFunc.pUserData);
			break;
	case SXFMT_CONS_BLOB:
			/* Blob consumer */
			rc = SyBlobAppend(pConsumer->uConsumer.pBlob,pSrc,(sxu32)nLen);
			break;
		default:
			/* Unknown consumer */
			break;
	}
	/* Update total number of bytes consumed so far */
	pConsumer->nLen += nLen;
	pConsumer->rc = rc;
	return rc;
}
static sxi32 FormatMount(sxi32 nType,void *pConsumer,ProcConsumer xUserCons,void *pUserData,sxu32 *pOutLen,const char *zFormat,va_list ap)
{
	SyFmtConsumer sCons;
	sCons.nType = nType;
	sCons.rc = SXRET_OK;
	sCons.nLen = 0;
	if( pOutLen ){
		*pOutLen = 0;
	}
	switch(nType){
	case SXFMT_CONS_PROC:
#if defined(UNTRUST)
			if( xUserCons == 0 ){
				return SXERR_EMPTY;
			}
#endif
			sCons.uConsumer.sFunc.xUserConsumer = xUserCons;
			sCons.uConsumer.sFunc.pUserData	    = pUserData;
		break;
		case SXFMT_CONS_BLOB:
			sCons.uConsumer.pBlob = (SyBlob *)pConsumer;
			break;
		default:
			return SXERR_UNKNOWN;
	}
	InternFormat(FormatConsumer,&sCons,zFormat,ap);
	if( pOutLen ){
		*pOutLen = sCons.nLen;
	}
	return sCons.rc;
}
PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...)
{
	va_list ap;
	sxi32 rc;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zFormat) ){
		return SXERR_EMPTY;
	}
#endif
	va_start(ap,zFormat);
	rc = FormatMount(SXFMT_CONS_PROC,0,xConsumer,pData,0,zFormat,ap);
	va_end(ap);
	return rc;
}
PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...)
{
	va_list ap;
	sxu32 n;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zFormat) ){
		return 0;
	}
#endif
	va_start(ap,zFormat);
	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);
	va_end(ap);
	return n;
}
PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap)
{
	sxu32 n = 0; /* cc warning */
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zFormat) ){
		return 0;
	}
#endif
	FormatMount(SXFMT_CONS_BLOB,&(*pBlob),0,0,&n,zFormat,ap);
	return n;
}
PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...)
{
	SyBlob sBlob;
	va_list ap;
	sxu32 n;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zFormat) ){
		return 0;
	}
#endif
	if( SXRET_OK != SyBlobInitFromBuf(&sBlob,zBuf,nLen - 1) ){
		return 0;
	}
	va_start(ap,zFormat);
	FormatMount(SXFMT_CONS_BLOB,&sBlob,0,0,0,zFormat,ap);
	va_end(ap);
	n = SyBlobLength(&sBlob);
	/* Append the null terminator */
	sBlob.mByte++;
	SyBlobAppend(&sBlob,"\0",sizeof(char));
	return n;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
* Symisc XML Parser Engine (UTF-8) SAX(Event Driven) API
* @author Mrad Chems Eddine <chm@symisc.net>
* @started 08/03/2010 21:32 FreeBSD
* @finished	07/04/2010 23:24 Win32[VS8]
*/
/*
 * An XML raw text,CDATA,tag name is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct SyXMLRawStrNS SyXMLRawStrNS;
struct SyXMLRawStrNS
{
	/* Public field [Must match the SyXMLRawStr fields ] */
	const char *zString; /* Raw text [UTF-8 ENCODED EXCEPT CDATA] [NOT NULL TERMINATED] */
	sxu32 nByte; /* Text length */
	sxu32 nLine; /* Line number this text occurs */
	/* Private fields */
	SySet sNSset; /* Namespace entries */
};
/*
 * Lexer token codes
 * The following set of constants are the token value recognized
 * by the lexer when processing XML input.
 */
#define SXML_TOK_INVALID	0xFFFF /* Invalid Token */
#define SXML_TOK_COMMENT	0x01   /* Comment */
#define SXML_TOK_PI	        0x02   /* Processing instruction */
#define SXML_TOK_DOCTYPE	0x04   /* Doctype directive */
#define SXML_TOK_RAW		0x08   /* Raw text */
#define SXML_TOK_START_TAG	0x10   /* Starting tag */
#define SXML_TOK_CDATA		0x20   /* CDATA */
#define SXML_TOK_END_TAG	0x40   /* Ending tag */
#define SXML_TOK_START_END	0x80   /* Tag */
#define SXML_TOK_SPACE		0x100  /* Spaces (including new lines) */
#define IS_XML_DIRTY(c) \
	( c == '<' || c == '$'|| c == '"' || c == '\''|| c == '&'|| c == '(' || c == ')' || c == '*' ||\
	c == '%'  || c == '#' || c == '|' || c == '/'|| c == '~' || c == '{' || c == '}' ||\
	c == '['  || c == ']' || c == '\\'|| c == ';'||c == '^'  || c == '`' )
/* Tokenize an entire XML input */
static sxi32 XML_Tokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pUnused2)
{
	SyXMLParser *pParse = (SyXMLParser *)pUserData;
	SyString *pStr;
	sxi32 rc;
	int c;
	/* Jump leading white spaces */
	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){
		/* Advance the stream cursor */
		if( pStream->zText[0] == '\n' ){
			/* Increment line counter */
			pStream->nLine++;
		}
		pStream->zText++;
	}
	if( pStream->zText >= pStream->zEnd ){
		SXUNUSED(pUnused2);
		/* End of input reached */
		return SXERR_EOF;
	}
	/* Record token starting position and line */
	pToken->nLine = pStream->nLine;
	pToken->pUserData = 0;
	pStr = &pToken->sData;
	SyStringInitFromBuf(pStr,pStream->zText,0);
	/* Extract the current token */
	c = pStream->zText[0];
	if( c == '<' ){
		pStream->zText++;
		pStr->zString++;
		if( pStream->zText >= pStream->zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* End of input reached */
			return SXERR_EOF;
		}
		c = pStream->zText[0];
		if( c == '?' ){
			/* Processing instruction */
			pStream->zText++;
			pStr->zString++;
			pToken->nType = SXML_TOK_PI;
			while( XLEX_IN_LEN(pStream) >= sizeof("?>")-1 &&
				SyMemcmp((const void *)pStream->zText,"?>",sizeof("?>")-1) != 0 ){
					if( pStream->zText[0] == '\n' ){
						/* Increment line counter */
						pStream->nLine++;
					}
					pStream->zText++;
			}
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			if( XLEX_IN_LEN(pStream) < sizeof("?>")-1 ){
				if( pParse->xError ){
					rc = pParse->xError("End of input found,but processing instruction was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				return SXERR_EOF;
			}
			pStream->zText += sizeof("?>")-1;
		}else if( c == '!' ){
			pStream->zText++;
			if( XLEX_IN_LEN(pStream) >= sizeof("--")-1 && pStream->zText[0] == '-' && pStream->zText[1] == '-' ){
				/* Comment */
				pStream->zText += sizeof("--") - 1;
				while( XLEX_IN_LEN(pStream) >= sizeof("-->")-1 &&
					SyMemcmp((const void *)pStream->zText,"-->",sizeof("-->")-1) != 0 ){
						if( pStream->zText[0] == '\n' ){
							/* Increment line counter */
							pStream->nLine++;
						}
						pStream->zText++;
				}
				pStream->zText += sizeof("-->")-1;
				/* Tell the lexer to ignore this token */
				return SXERR_CONTINUE;
			}
			if( XLEX_IN_LEN(pStream) >= sizeof("[CDATA[") - 1 && SyMemcmp((const void *)pStream->zText,"[CDATA[",sizeof("[CDATA[")-1) == 0 ){
				/* CDATA */
				pStream->zText += sizeof("[CDATA[") - 1;
				pStr->zString = (const char *)pStream->zText;
				while( XLEX_IN_LEN(pStream) >= sizeof("]]>")-1 &&
					SyMemcmp((const void *)pStream->zText,"]]>",sizeof("]]>")-1) != 0 ){
						if( pStream->zText[0] == '\n' ){
							/* Increment line counter */
							pStream->nLine++;
						}
						pStream->zText++;
				}
				/* Record token type and length */
				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
				pToken->nType = SXML_TOK_CDATA;
				if( XLEX_IN_LEN(pStream) < sizeof("]]>")-1 ){
					if( pParse->xError ){
						rc = pParse->xError("End of input found,but ]]> was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				pStream->zText += sizeof("]]>")-1;
				return SXRET_OK;
			}
			if( XLEX_IN_LEN(pStream) >= sizeof("DOCTYPE") - 1 && SyMemcmp((const void *)pStream->zText,"DOCTYPE",sizeof("DOCTYPE")-1) == 0 ){
				SyString sDelim = { ">" , sizeof(char) }; /* Default delimiter */
				int c = 0;
				/* DOCTYPE */
				pStream->zText += sizeof("DOCTYPE") - 1;
				pStr->zString = (const char *)pStream->zText;
				/* Check for element declaration */
				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){
					if( pStream->zText[0] >= 0xc0 || !SyisSpace(pStream->zText[0]) ){
						c = pStream->zText[0];
						if( c == '>' ){
							break;
						}
					}
					pStream->zText++;
				}
				if( c == '[' ){
					/* Change the delimiter */
					SyStringInitFromBuf(&sDelim,"]>",sizeof("]>")-1);
				}
				if( c != '>' ){
					while( XLEX_IN_LEN(pStream) >= sDelim.nByte &&
						SyMemcmp((const void *)pStream->zText,sDelim.zString,sDelim.nByte) != 0 ){
							if( pStream->zText[0] == '\n' ){
								/* Increment line counter */
								pStream->nLine++;
							}
							pStream->zText++;
					}
				}
				/* Record token type and length */
				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
				pToken->nType = SXML_TOK_DOCTYPE;
				if( XLEX_IN_LEN(pStream) < sDelim.nByte ){
					if( pParse->xError ){
						rc = pParse->xError("End of input found,but ]> or > was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				pStream->zText += sDelim.nByte;
				return SXRET_OK;
			}
		}else{
			int c;
			c = pStream->zText[0];
			rc = SXRET_OK;
			pToken->nType = SXML_TOK_START_TAG;
			if( c == '/' ){
				/* End tag */
				pToken->nType = SXML_TOK_END_TAG;
				pStream->zText++;
				pStr->zString++;
				if( pStream->zText >= pStream->zEnd ){
					if( pParse->xError ){
						rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				c = pStream->zText[0];
			}
			if( c == '>' ){
				/*<>*/
				if( pParse->xError ){
					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				/* Ignore the token */
				return SXERR_CONTINUE;
			}
			if( c < 0xc0 && (SyisSpace(c) || SyisDigit(c) || c == '.' || c == '-' ||IS_XML_DIRTY(c) ) ){
				if( pParse->xError ){
					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				rc = SXERR_INVALID;
			}
			pStream->zText++;
			/* Delimit the tag */
			while( pStream->zText < pStream->zEnd && pStream->zText[0] != '>' ){
				c = pStream->zText[0];
				if( c >= 0xc0 ){
					/* UTF-8 stream */
					pStream->zText++;
					SX_JMP_UTF8(pStream->zText,pStream->zEnd);
				}else{
					if( c == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '>' ){
						pStream->zText++;
						if( pToken->nType != SXML_TOK_START_TAG ){
							if( pParse->xError ){
								rc = pParse->xError("Unexpected closing tag,expecting '>'",
									SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
								if( rc == SXERR_ABORT ){
									return SXERR_ABORT;
								}
							}
							/* Ignore the token */
							rc = SXERR_INVALID;
						}else{
							pToken->nType = SXML_TOK_START_END;
						}
						break;
					}
					if( pStream->zText[0] == '\n' ){
						/* Increment line counter */
						pStream->nLine++;
					}
					/* Advance the stream cursor */
					pStream->zText++;
				}
			}
			if( rc != SXRET_OK ){
				/* Tell the lexer to ignore this token */
				return SXERR_CONTINUE;
			}
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			if( pToken->nType == SXML_TOK_START_END && pStr->nByte > 0){
				pStr->nByte -= sizeof(char);
			}
			if ( pStream->zText < pStream->zEnd ){
				pStream->zText++;
			}else{
				if( pParse->xError ){
					rc = pParse->xError("End of input found,but closing tag '>' was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
		}
	}else{
		/* Raw input */
		while( pStream->zText < pStream->zEnd ){
			c = pStream->zText[0];
			if( c < 0xc0 ){
				if( c == '<' ){
					break;
				}else if( c == '\n' ){
					/* Increment line counter */
					pStream->nLine++;
				}
				/* Advance the stream cursor */
				pStream->zText++;
			}else{
				/* UTF-8 stream */
				pStream->zText++;
				SX_JMP_UTF8(pStream->zText,pStream->zEnd);
			}
		}
		/* Record token type,length */
		pToken->nType = SXML_TOK_RAW;
		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
	}
	/* Return to the lexer */
	return SXRET_OK;
}
static int XMLCheckDuplicateAttr(SyXMLRawStr *aSet,sxu32 nEntry,SyXMLRawStr *pEntry)
{
	sxu32 n;
	for( n = 0 ; n < nEntry ; n += 2 ){
		SyXMLRawStr *pAttr = &aSet[n];
		if( pAttr->nByte == pEntry->nByte && SyMemcmp(pAttr->zString,pEntry->zString,pEntry->nByte) == 0 ){
			/* Attribute found */
			return 1;
		}
	}
	/* No duplicates */
	return 0;
}
static sxi32 XMLProcessNamesSpace(SyXMLParser *pParse,SyXMLRawStrNS *pTag,SyToken *pToken,SySet *pAttr)
{
	SyXMLRawStr *pPrefix,*pUri; /* Namespace prefix/URI */
	SyHashEntry *pEntry;
	SyXMLRawStr *pDup;
	sxi32 rc;
	/* Extract the URI first */
	pUri = (SyXMLRawStr *)SySetPeek(pAttr);
	/* Extract the prefix */
	pPrefix =  (SyXMLRawStr *)SySetAt(pAttr,SySetUsed(pAttr) - 2);
	/* Prefix name */
	if( pPrefix->nByte == sizeof("xmlns")-1 ){
		/* Default namespace */
		pPrefix->nByte = 0;
		pPrefix->zString = ""; /* Empty string */
	}else{
		pPrefix->nByte   -= sizeof("xmlns")-1;
		pPrefix->zString += sizeof("xmlns")-1;
		if( pPrefix->zString[0] != ':' ){
			return SXRET_OK;
		}
		pPrefix->nByte--;
		pPrefix->zString++;
		if( pPrefix->nByte < 1 ){
			if( pParse->xError ){
				rc = pParse->xError("Invalid namespace name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* POP the last inserted two entries */
			(void)SySetPop(pAttr);
			(void)SySetPop(pAttr);
			return SXERR_SYNTAX;
		}
	}
	/* Invoke the namespace callback if available */
	if( pParse->xNameSpace ){
		rc = pParse->xNameSpace(pPrefix,pUri,pParse->pUserData);
		if( rc == SXERR_ABORT ){
			/* User callback request an operation abort */
			return SXERR_ABORT;
		}
	}
	/* Duplicate structure */
	pDup = (SyXMLRawStr *)SyMemBackendAlloc(pParse->pAllocator,sizeof(SyXMLRawStr));
	if( pDup == 0 ){
		if( pParse->xError ){
			pParse->xError("Out of memory",SXML_ERROR_NO_MEMORY,pToken,pParse->pUserData);
		}
		/* Abort processing immediately */
		return SXERR_ABORT;
	}
	*pDup = *pUri; /* Structure assignement */
	/* Save the namespace */
	if( pPrefix->nByte == 0 ){
		pPrefix->zString = "Default";
		pPrefix->nByte = sizeof("Default")-1;
	}
	SyHashInsert(&pParse->hns,(const void *)pPrefix->zString,pPrefix->nByte,pDup);
	/* Peek the last inserted entry */
	pEntry = SyHashLastEntry(&pParse->hns);
	/* Store in the corresponding tag container*/
	SySetPut(&pTag->sNSset,(const void *)&pEntry);
	/* POP the last inserted two entries */
	(void)SySetPop(pAttr);
	(void)SySetPop(pAttr);
	return SXRET_OK;
}
static sxi32 XMLProcessStartTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SySet  *pAttrSet,SySet *pTagStack)
{
	SyString *pIn = &pToken->sData;
	const char *zIn,*zCur,*zEnd;
	SyXMLRawStr sEntry;
	sxi32 rc;
	int c;
	/* Reset the working set */
	SySetReset(pAttrSet);
	/* Delimit the raw tag */
	zIn = pIn->zString;
	zEnd = &zIn[pIn->nByte];
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	/* Isolate tag name */
	sEntry.nLine = pTag->nLine = pToken->nLine;
	zCur = zIn;
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else if( SyisSpace(zIn[0])){
			break;
		}else{
			if( IS_XML_DIRTY(zIn[0]) ){
				if( pParse->xError ){
					rc = pParse->xError("Illegal character in XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
			zIn++;
		}
	}
	if( zCur >= zIn ){
		if( pParse->xError ){
			rc = pParse->xError("Invalid XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	pTag->zString = zCur;
	pTag->nByte = (sxu32)(zIn-zCur);
	/* Process tag attribute */
	for(;;){
		int is_ns = 0;
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd ){
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '=' ){
			if( (unsigned char)zIn[0] >= 0xc0 ){
				/* UTF-8 stream */
				zIn++;
				SX_JMP_UTF8(zIn,zEnd);
			}else if( SyisSpace(zIn[0]) ){
				break;
			}else{
				zIn++;
			}
		}
		if( zCur >= zIn ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		/* Store attribute name */
		sEntry.zString = zCur;
		sEntry.nByte = (sxu32)(zIn-zCur);
		if( (pParse->nFlags & SXML_ENABLE_NAMESPACE) && sEntry.nByte >= sizeof("xmlns") - 1 &&
			SyMemcmp(sEntry.zString,"xmlns",sizeof("xmlns") - 1) == 0 ){
				is_ns = 1;
		}
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd || zIn[0] != '=' ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		while( sEntry.nByte > 0 && (unsigned char)zCur[sEntry.nByte - 1] < 0xc0
			&& SyisSpace(zCur[sEntry.nByte - 1])){
				sEntry.nByte--;
		}
		/* Check for duplicates first */
		if( XMLCheckDuplicateAttr((SyXMLRawStr *)SySetBasePtr(pAttrSet),SySetUsed(pAttrSet),&sEntry) ){
			if( pParse->xError ){
				rc = pParse->xError("Duplicate attribute",SXML_ERROR_DUPLICATE_ATTRIBUTE,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){
			return SXERR_ABORT;
		}
		/* Extract attribute value */
		zIn++; /* Jump the trailing '=' */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		if( zIn[0] != '\'' && zIn[0] != '"' ){
			if( pParse->xError ){
				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		c = zIn[0];
		zIn++;
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != c ){
			zIn++;
		}
		if( zIn >= zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		/* Store attribute value */
		sEntry.zString = zCur;
		sEntry.nByte = (sxu32)(zIn-zCur);
		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){
			return SXERR_ABORT;
		}
		zIn++;
		if( is_ns ){
			/* Process namespace declaration */
			XMLProcessNamesSpace(pParse,pTag,pToken,pAttrSet);
		}
	}
	/* Store in the tag stack */
	if( pToken->nType == SXML_TOK_START_TAG ){
		rc = SySetPut(pTagStack,(const void *)pTag);
	}
	return SXRET_OK;
}
static void XMLExtactPI(SyToken *pToken,SyXMLRawStr *pTarget,SyXMLRawStr *pData,int *pXML)
{
	SyString *pIn = &pToken->sData;
	const char *zIn,*zCur,*zEnd;

	pTarget->nLine = pData->nLine = pToken->nLine;
	/* Nullify the entries first */
	pTarget->zString = pData->zString = 0;
	/* Ignore leading and trailing white spaces */
	SyStringFullTrim(pIn);
	/* Delimit the raw PI */
	zIn  = pIn->zString;
	zEnd = &zIn[pIn->nByte];
	if( pXML ){
		*pXML = 0;
	}
	/* Extract the target */
	zCur = zIn;
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else if( SyisSpace(zIn[0])){
			break;
		}else{
			zIn++;
		}
	}
	if( zIn > zCur ){
		pTarget->zString = zCur;
		pTarget->nByte = (sxu32)(zIn-zCur);
		if( pXML && pTarget->nByte == sizeof("xml")-1 && SyStrnicmp(pTarget->zString,"xml",sizeof("xml")-1) == 0 ){
			*pXML = 1;
		}
	}
	/* Extract the PI data  */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	if( zIn < zEnd ){
		pData->zString = zIn;
		pData->nByte = (sxu32)(zEnd-zIn);
	}
}
static sxi32 XMLExtractEndTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pOut)
{
	SyString *pIn = &pToken->sData;
	const char *zEnd = &pIn->zString[pIn->nByte];
	const char *zIn = pIn->zString;
	/* Ignore leading white spaces */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	pOut->nLine = pToken->nLine;
	pOut->zString = zIn;
	pOut->nByte = (sxu32)(zEnd-zIn);
	/* Ignore trailing white spaces */
	while( pOut->nByte > 0 && (unsigned char)pOut->zString[pOut->nByte - 1] < 0xc0
		&& SyisSpace(pOut->zString[pOut->nByte - 1]) ){
			pOut->nByte--;
	}
	if( pOut->nByte < 1 ){
		if( pParse->xError ){
			sxi32 rc;
			rc  = pParse->xError("Invalid end tag name",SXML_ERROR_INVALID_TOKEN,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	return SXRET_OK;
}
static void TokenToXMLString(SyToken *pTok,SyXMLRawStrNS *pOut)
{
	/* Remove leading and trailing white spaces first */
	SyStringFullTrim(&pTok->sData);
	pOut->zString = SyStringData(&pTok->sData);
	pOut->nByte = SyStringLength(&pTok->sData);
}
static sxi32 XMLExtractNS(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SyXMLRawStr *pnsUri)
{
	SyXMLRawStr *pUri,sPrefix;
	SyHashEntry *pEntry;
	sxu32 nOfft;
	sxi32 rc;
	/* Extract a prefix if available */
	rc = SyByteFind(pTag->zString,pTag->nByte,':',&nOfft);
	if( rc != SXRET_OK ){
		/* Check if there is a default namespace */
		pEntry = SyHashGet(&pParse->hns,"Default",sizeof("Default")-1);
		if( pEntry  ){
			/* Extract the ns URI */
			pUri = (SyXMLRawStr *)pEntry->pUserData;
			/* Save the ns URI */
			pnsUri->zString = pUri->zString;
			pnsUri->nByte = pUri->nByte;
		}
		return SXRET_OK;
	}
	if( nOfft < 1 ){
		if( pParse->xError ){
			rc = pParse->xError("Empty prefix is not allowed according to XML namespace specification",
				SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	sPrefix.zString = pTag->zString;
	sPrefix.nByte = nOfft;
	sPrefix.nLine = pTag->nLine;
	pTag->zString += nOfft + 1;
	pTag->nByte -= nOfft;
	if( pTag->nByte < 1 ){
		if( pParse->xError ){
			rc = pParse->xError("Missing tag name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	/* Check if the prefix is already registered */
	pEntry = SyHashGet(&pParse->hns,sPrefix.zString,sPrefix.nByte);
	if( pEntry == 0 ){
		if( pParse->xError ){
			rc = pParse->xError("Namespace prefix is not defined",SXML_ERROR_SYNTAX,
				pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	/* Extract the ns URI */
	pUri = (SyXMLRawStr *)pEntry->pUserData;
	/* Save the ns URI */
	pnsUri->zString = pUri->zString;
	pnsUri->nByte = pUri->nByte;
	/* All done */
	return SXRET_OK;
}
static sxi32 XMLnsUnlink(SyXMLParser *pParse,SyXMLRawStrNS *pLast,SyToken *pToken)
{
	SyHashEntry **apEntry,*pEntry;
	void *pUserData;
	sxu32 n;
	/* Release namespace entries */
	apEntry = (SyHashEntry **)SySetBasePtr(&pLast->sNSset);
	for( n = 0 ; n < SySetUsed(&pLast->sNSset) ; ++n ){
		pEntry = apEntry[n];
		/* Invoke the end namespace declaration callback */
		if( pParse->xNameSpaceEnd && (pParse->nFlags & SXML_ENABLE_NAMESPACE) && pToken ){
			SyXMLRawStr sPrefix;
			sxi32 rc;
			sPrefix.zString = (const char *)pEntry->pKey;
			sPrefix.nByte = pEntry->nKeyLen;
			sPrefix.nLine = pToken->nLine;
			rc = pParse->xNameSpaceEnd(&sPrefix,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		pUserData = pEntry->pUserData;
		/* Remove from the namespace hashtable */
		SyHashDeleteEntry2(pEntry);
		SyMemBackendFree(pParse->pAllocator,pUserData);
	}
	SySetRelease(&pLast->sNSset);
	return SXRET_OK;
}
/* Process XML tokens */
static sxi32  ProcessXML(SyXMLParser *pParse,SySet *pTagStack,SySet *pWorker)
{
	SySet *pTokenSet = &pParse->sToken;
	SyXMLRawStrNS sEntry;
	SyXMLRawStr sNs;
	SyToken *pToken;
	int bGotTag;
	sxi32 rc;
	/* Initialize fields */
	bGotTag = 0;
	/* Start processing */
	if( pParse->xStartDoc && (SXERR_ABORT == pParse->xStartDoc(pParse->pUserData)) ){
		/* User callback request an operation abort */
		return SXERR_ABORT;
	}
	/* Reset the loop cursor */
	SySetResetCursor(pTokenSet);
	/* Extract the current token */
	while( SXRET_OK == (SySetGetNextEntry(&(*pTokenSet),(void **)&pToken)) ){
		SyZero(&sEntry,sizeof(SyXMLRawStrNS));
		SyZero(&sNs,sizeof(SyXMLRawStr));
		SySetInit(&sEntry.sNSset,pParse->pAllocator,sizeof(SyHashEntry *));
		sEntry.nLine = sNs.nLine = pToken->nLine;
		switch(pToken->nType){
		case SXML_TOK_DOCTYPE:
			if( SySetUsed(pTagStack) > 1 || bGotTag ){
				if( pParse->xError ){
					rc = pParse->xError("DOCTYPE must be declared first",SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			/* Invoke the supplied callback if any */
			if( pParse->xDoctype ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xDoctype((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_CDATA:
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("CDATA without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
			/* Invoke the supplied callback if any */
			if( pParse->xRaw ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_PI:{
			SyXMLRawStr sTarget,sData;
			int isXML = 0;
			/* Extract the target and data */
			XMLExtactPI(pToken,&sTarget,&sData,&isXML);
			if( isXML && SySetCursor(pTokenSet) > 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Unexpected XML declaration. The XML declaration must be the first node in the document",
						SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}else if( pParse->xPi ){
				/* Invoke the supplied callback*/
				rc = pParse->xPi(&sTarget,&sData,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
						 }
		case SXML_TOK_RAW:
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Text (Raw data) without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			/* Invoke the supplied callback if any */
			if( pParse->xRaw ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_END_TAG:{
			SyXMLRawStrNS *pLast = 0; /* cc warning */
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			rc = XMLExtractEndTag(pParse,pToken,&sEntry);
			if( rc == SXRET_OK ){
				/* Extract the last inserted entry */
				pLast = (SyXMLRawStrNS *)SySetPeek(pTagStack);
				if( pLast == 0 || pLast->nByte != sEntry.nByte ||
					SyMemcmp(pLast->zString,sEntry.zString,sEntry.nByte) != 0 ){
						if( pParse->xError ){
							rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
				}else{
					/* Invoke the supplied callback if any */
					if( pParse->xEndTag ){
						rc = SXRET_OK;
						if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){
							/* Extract namespace URI */
							rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
						if( rc == SXRET_OK ){
							rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
					}
				}
			}else if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( pLast ){
				rc = XMLnsUnlink(pParse,pLast,pToken);
				(void)SySetPop(pTagStack);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
							  }
		case SXML_TOK_START_TAG:
		case SXML_TOK_START_END:
			if( SySetUsed(pTagStack) < 1 && bGotTag ){
				if( pParse->xError ){
					rc = pParse->xError("XML document cannot contain multiple root level elements documents",
						SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			bGotTag = 1;
			/* Extract the tag and it's supplied attribute */
			rc = XMLProcessStartTag(pParse,pToken,&sEntry,pWorker,pTagStack);
			if( rc == SXRET_OK ){
				if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){
					/* Extract namespace URI */
					rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);
				}
			}
			if( rc == SXRET_OK ){
				/* Invoke the supplied callback */
				if( pParse->xStartTag ){
					rc = pParse->xStartTag((SyXMLRawStr *)&sEntry,&sNs,SySetUsed(pWorker),
						(SyXMLRawStr *)SySetBasePtr(pWorker),pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				if( pToken->nType == SXML_TOK_START_END ){
					if ( pParse->xEndTag ){
						rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					rc = XMLnsUnlink(pParse,&sEntry,pToken);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}else if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			break;
		default:
			/* Can't happen */
			break;
		}
	}
	if( SySetUsed(pTagStack) > 0 && pParse->xError){
		pParse->xError("Missing closing tag",SXML_ERROR_SYNTAX,
			(SyToken *)SySetPeek(&pParse->sToken),pParse->pUserData);
	}
	if( pParse->xEndDoc ){
		pParse->xEndDoc(pParse->pUserData);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyXMLParserInit(SyXMLParser *pParser,SyMemBackend *pAllocator,sxi32 iFlags)
{
	/* Zero the structure first */
	SyZero(pParser,sizeof(SyXMLParser));
	/* Initialize fields */
	SySetInit(&pParser->sToken,pAllocator,sizeof(SyToken));
	SyLexInit(&pParser->sLex,&pParser->sToken,XML_Tokenize,pParser);
	SyHashInit(&pParser->hns,pAllocator,0,0);
	pParser->pAllocator = pAllocator;
	pParser->nFlags = iFlags;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(SyXMLParser *pParser,
	void *pUserData,
	ProcXMLStartTagHandler xStartTag,
	ProcXMLTextHandler xRaw,
	ProcXMLSyntaxErrorHandler xErr,
	ProcXMLStartDocument xStartDoc,
	ProcXMLEndTagHandler xEndTag,
	ProcXMLPIHandler   xPi,
	ProcXMLEndDocument xEndDoc,
	ProcXMLDoctypeHandler xDoctype,
	ProcXMLNameSpaceStart xNameSpace,
	ProcXMLNameSpaceEnd   xNameSpaceEnd
	){
	/* Install user callbacks */
	if( xErr ){
		pParser->xError = xErr;
	}
	if( xStartDoc ){
		pParser->xStartDoc = xStartDoc;
	}
	if( xStartTag ){
		pParser->xStartTag = xStartTag;
	}
	if( xRaw ){
		pParser->xRaw = xRaw;
	}
	if( xEndTag ){
		pParser->xEndTag = xEndTag;
	}
	if( xPi ){
		pParser->xPi = xPi;
	}
	if( xEndDoc ){
		pParser->xEndDoc = xEndDoc;
	}
	if( xDoctype ){
		pParser->xDoctype = xDoctype;
	}
	if( xNameSpace ){
		pParser->xNameSpace	= xNameSpace;
	}
	if( xNameSpaceEnd ){
		pParser->xNameSpaceEnd = xNameSpaceEnd;
	}
	pParser->pUserData = pUserData;
	return SXRET_OK;
}
/* Process an XML chunk */
PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser *pParser,const char *zInput,sxu32 nByte)
{
	SySet sTagStack;
	SySet sWorker;
	sxi32 rc;
	/* Initialize working sets */
	SySetInit(&sWorker,pParser->pAllocator,sizeof(SyXMLRawStr)); /* Tag container */
	SySetInit(&sTagStack,pParser->pAllocator,sizeof(SyXMLRawStrNS)); /* Tag stack */
	/* Tokenize the entire input */
	rc = SyLexTokenizeInput(&pParser->sLex,zInput,nByte,0,0,0);
	if( rc == SXERR_ABORT ){
		/* Tokenize callback request an operation abort */
		return SXERR_ABORT;
	}
	if( SySetUsed(&pParser->sToken) < 1 ){
		/* Nothing to process [i.e: white spaces] */
		rc = SXRET_OK;
	}else{
		/* Process XML Tokens */
		rc = ProcessXML(&(*pParser),&sTagStack,&sWorker);
		if( pParser->nFlags & SXML_ENABLE_NAMESPACE ){
			if( SySetUsed(&sTagStack) > 0  ){
				SyXMLRawStrNS *pEntry;
				SyHashEntry **apEntry;
				sxu32 n;
				SySetResetCursor(&sTagStack);
				while( SySetGetNextEntry(&sTagStack,(void **)&pEntry) == SXRET_OK ){
					/* Release namespace entries */
					apEntry = (SyHashEntry **)SySetBasePtr(&pEntry->sNSset);
					for( n = 0 ; n < SySetUsed(&pEntry->sNSset) ; ++n ){
						SyMemBackendFree(pParser->pAllocator,apEntry[n]->pUserData);
					}
					SySetRelease(&pEntry->sNSset);
				}
			}
		}
	}
	/* Clean-up the mess left behind */
	SySetRelease(&sWorker);
	SySetRelease(&sTagStack);
	/* Processing result */
	return rc;
}
PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser *pParser)
{
	SyLexRelease(&pParser->sLex);
	SySetRelease(&pParser->sToken);
	SyHashRelease(&pParser->hns);
	return SXRET_OK;
}
/*
 * Zip File Format:
 *
 * Byte order: Little-endian
 *
 * [Local file header + Compressed data [+ Extended local header]?]*
 * [Central directory]*
 * [End of central directory record]
 *
 * Local file header:*
 * Offset   Length   Contents
 *  0      4 bytes  Local file header signature (0x04034b50)
 *  4      2 bytes  Version needed to extract
 *  6      2 bytes  General purpose bit flag
 *  8      2 bytes  Compression method
 * 10      2 bytes  Last mod file time
 * 12      2 bytes  Last mod file date
 * 14      4 bytes  CRC-32
 * 18      4 bytes  Compressed size (n)
 * 22      4 bytes  Uncompressed size
 * 26      2 bytes  Filename length (f)
 * 28      2 bytes  Extra field length (e)
 * 30     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (n)bytes  Compressed data
 *
 * Extended local header:*
 * Offset   Length   Contents
 *  0      4 bytes  Extended Local file header signature (0x08074b50)
 *  4      4 bytes  CRC-32
 *  8      4 bytes  Compressed size
 * 12      4 bytes  Uncompressed size
 *
 * Extra field:?(if any)
 * Offset 	Length		Contents
 * 0	  	2 bytes		Header ID (0x001 until 0xfb4a) see extended appnote from Info-zip
 * 2	  	2 bytes		Data size (g)
 * 		  	(g) bytes	(g) bytes of extra field
 *
 * Central directory:*
 * Offset   Length   Contents
 *  0      4 bytes  Central file header signature (0x02014b50)
 *  4      2 bytes  Version made by
 *  6      2 bytes  Version needed to extract
 *  8      2 bytes  General purpose bit flag
 * 10      2 bytes  Compression method
 * 12      2 bytes  Last mod file time
 * 14      2 bytes  Last mod file date
 * 16      4 bytes  CRC-32
 * 20      4 bytes  Compressed size
 * 24      4 bytes  Uncompressed size
 * 28      2 bytes  Filename length (f)
 * 30      2 bytes  Extra field length (e)
 * 32      2 bytes  File comment length (c)
 * 34      2 bytes  Disk number start
 * 36      2 bytes  Internal file attributes
 * 38      4 bytes  External file attributes
 * 42      4 bytes  Relative offset of local header
 * 46     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (c)bytes  File comment
 *
 * End of central directory record:
 * Offset   Length   Contents
 *  0      4 bytes  End of central dir signature (0x06054b50)
 *  4      2 bytes  Number of this disk
 *  6      2 bytes  Number of the disk with the start of the central directory
 *  8      2 bytes  Total number of entries in the central dir on this disk
 * 10      2 bytes  Total number of entries in the central dir
 * 12      4 bytes  Size of the central directory
 * 16      4 bytes  Offset of start of central directory with respect to the starting disk number
 * 20      2 bytes  zipfile comment length (c)
 * 22     (c)bytes  zipfile comment
 *
 * compression method: (2 bytes)
 *          0 - The file is stored (no compression)
 *          1 - The file is Shrunk
 *          2 - The file is Reduced with compression factor 1
 *          3 - The file is Reduced with compression factor 2
 *          4 - The file is Reduced with compression factor 3
 *          5 - The file is Reduced with compression factor 4
 *          6 - The file is Imploded
 *          7 - Reserved for Tokenizing compression algorithm
 *          8 - The file is Deflated
 */

#define SXMAKE_ZIP_WORKBUF	(SXU16_HIGH/2)	/* 32KB Initial working buffer size */
#define SXMAKE_ZIP_EXTRACT_VER	0x000a	/* Version needed to extract */
#define SXMAKE_ZIP_VER	0x003	/* Version made by */

#define SXZIP_CENTRAL_MAGIC			0x02014b50
#define SXZIP_END_CENTRAL_MAGIC		0x06054b50
#define SXZIP_LOCAL_MAGIC			0x04034b50
/*#define SXZIP_CRC32_START			0xdebb20e3*/

#define SXZIP_LOCAL_HDRSZ		30	/* Local header size */
#define SXZIP_LOCAL_EXT_HDRZ	16	/* Extended local header(footer) size */
#define SXZIP_CENTRAL_HDRSZ		46	/* Central directory header size */
#define SXZIP_END_CENTRAL_HDRSZ	22	/* End of central directory header size */

#define SXARCHIVE_HASH_SIZE	64 /* Starting hash table size(MUST BE POWER OF 2)*/
static sxi32 SyLittleEndianUnpack32(sxu32 *uNB,const unsigned char *buf,sxu32 Len)
{
	if( Len < sizeof(sxu32) ){
		return SXERR_SHORT;
	}
	*uNB =  buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
	return SXRET_OK;
}
static sxi32 SyLittleEndianUnpack16(sxu16 *pOut,const unsigned char *zBuf,sxu32 nLen)
{
	if( nLen < sizeof(sxu16) ){
		return SXERR_SHORT;
	}
	*pOut = zBuf[0] + (zBuf[1] <<8);

	return SXRET_OK;
}
static sxi32 SyDosTimeFormat(sxu32 nDosDate,Sytm *pOut)
{
	sxu16 nDate;
	sxu16 nTime;
	nDate = nDosDate >> 16;
	nTime = nDosDate & 0xFFFF;
	pOut->tm_isdst  = 0;
	pOut->tm_year 	= 1980 + (nDate >> 9);
	pOut->tm_mon	= (nDate % (1<<9))>>5;
	pOut->tm_mday	= (nDate % (1<<9))&0x1F;
	pOut->tm_hour	= nTime >> 11;
	pOut->tm_min	= (nTime % (1<<11)) >> 5;
	pOut->tm_sec	= ((nTime % (1<<11))& 0x1F )<<1;
	return SXRET_OK;
}
/*
 * Archive hashtable manager
 */
static sxi32 ArchiveHashGetEntry(SyArchive *pArch,const char *zName,sxu32 nLen,SyArchiveEntry **ppEntry)
{
	SyArchiveEntry *pBucketEntry;
	SyString sEntry;
	sxu32 nHash;

	nHash = pArch->xHash(zName,nLen);
	pBucketEntry = pArch->apHash[nHash & (pArch->nSize - 1)];

	SyStringInitFromBuf(&sEntry,zName,nLen);

	for(;;){
		if( pBucketEntry == 0 ){
			break;
		}
		if( nHash == pBucketEntry->nHash && pArch->xCmp(&sEntry,&pBucketEntry->sFileName) == 0 ){
			if( ppEntry ){
				*ppEntry = pBucketEntry;
			}
			return SXRET_OK;
		}
		pBucketEntry = pBucketEntry->pNextHash;
	}
	return SXERR_NOTFOUND;
}
static void ArchiveHashBucketInstall(SyArchiveEntry **apTable,sxu32 nBucket,SyArchiveEntry *pEntry)
{
	pEntry->pNextHash = apTable[nBucket];
	if( apTable[nBucket] != 0 ){
		apTable[nBucket]->pPrevHash = pEntry;
	}
	apTable[nBucket] = pEntry;
}
static sxi32 ArchiveHashGrowTable(SyArchive *pArch)
{
	sxu32 nNewSize = pArch->nSize * 2;
	SyArchiveEntry **apNew;
	SyArchiveEntry *pEntry;
	sxu32 n;

	/* Allocate a new table */
	apNew = (SyArchiveEntry **)SyMemBackendAlloc(pArch->pAllocator,nNewSize * sizeof(SyArchiveEntry *));
	if( apNew == 0 ){
		return SXRET_OK; /* Not so fatal,simply a performance hit */
	}
	SyZero(apNew,nNewSize * sizeof(SyArchiveEntry *));
	/* Rehash old entries */
	for( n = 0 , pEntry = pArch->pList ; n < pArch->nLoaded ; n++ , pEntry = pEntry->pNext ){
		pEntry->pNextHash = pEntry->pPrevHash = 0;
		ArchiveHashBucketInstall(apNew,pEntry->nHash & (nNewSize - 1),pEntry);
	}
	/* Release the old table */
	SyMemBackendFree(pArch->pAllocator,pArch->apHash);
	pArch->apHash = apNew;
	pArch->nSize = nNewSize;

	return SXRET_OK;
}
static sxi32 ArchiveHashInstallEntry(SyArchive *pArch,SyArchiveEntry *pEntry)
{
	if( pArch->nLoaded > pArch->nSize * 3 ){
		ArchiveHashGrowTable(&(*pArch));
	}
	pEntry->nHash = pArch->xHash(SyStringData(&pEntry->sFileName),SyStringLength(&pEntry->sFileName));
	/* Install the entry in its bucket */
	ArchiveHashBucketInstall(pArch->apHash,pEntry->nHash & (pArch->nSize - 1),pEntry);
	MACRO_LD_PUSH(pArch->pList,pEntry);
	pArch->nLoaded++;

	return SXRET_OK;
}
 /*
  * Parse the End of central directory and report status
  */
 static sxi32 ParseEndOfCentralDirectory(SyArchive *pArch,const unsigned char *zBuf)
 {
	sxu32 nMagic = 0; /* cc -O6 warning */
 	sxi32 rc;

 	/* Sanity check */
 	rc = SyLittleEndianUnpack32(&nMagic,zBuf,sizeof(sxu32));
 	if( /* rc != SXRET_OK || */nMagic != SXZIP_END_CENTRAL_MAGIC ){
 		return SXERR_CORRUPT;
 	}
 	/* # of entries */
 	rc = SyLittleEndianUnpack16((sxu16 *)&pArch->nEntry,&zBuf[8],sizeof(sxu16));
 	if( /* rc != SXRET_OK || */ pArch->nEntry > SXI16_HIGH /* SXU16_HIGH */ ){
 		return SXERR_CORRUPT;
 	}
 	/* Size of central directory */
 	rc = SyLittleEndianUnpack32(&pArch->nCentralSize,&zBuf[12],sizeof(sxu32));
 	if( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH ){
 		return SXERR_CORRUPT;
 	}
 	/* Starting offset of central directory */
 	rc = SyLittleEndianUnpack32(&pArch->nCentralOfft,&zBuf[16],sizeof(sxu32));
 	if( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH ){
 		return SXERR_CORRUPT;
 	}

 	return SXRET_OK;
 }
 /*
  * Fill the zip entry with the appropriate information from the central directory
  */
static sxi32 GetCentralDirectoryEntry(SyArchive *pArch,SyArchiveEntry *pEntry,const unsigned char *zCentral,sxu32 *pNextOffset)
 {
 	SyString *pName = &pEntry->sFileName; /* File name */
 	sxu16 nDosDate,nDosTime;
	sxu16 nComment = 0 ;
	sxu32 nMagic = 0; /* cc -O6 warning */
	sxi32 rc;
	nDosDate = nDosTime = 0; /* cc -O6 warning */
	SXUNUSED(pArch);
 	/* Sanity check */
 	rc = SyLittleEndianUnpack32(&nMagic,zCentral,sizeof(sxu32));
 	if( /* rc != SXRET_OK || */ nMagic != SXZIP_CENTRAL_MAGIC ){
 		rc = SXERR_CORRUPT;
 		/*
 		 * Try to recover by examining the next central directory record.
 		 * Dont worry here, there is no risk of an infinite loop since
		 * the buffer size is delimited.
 		 */

 		/* pName->nByte = 0; nComment = 0; pName->nExtra = 0 */
 		goto update;
 	}
 	/*
 	 * entry name length
 	 */
 	SyLittleEndianUnpack16((sxu16 *)&pName->nByte,&zCentral[28],sizeof(sxu16));
 	if( pName->nByte > SXI16_HIGH /* SXU16_HIGH */){
 		 rc = SXERR_BIG;
 		 goto update;
 	}
 	/* Extra information */
 	SyLittleEndianUnpack16(&pEntry->nExtra,&zCentral[30],sizeof(sxu16));
 	/* Comment length  */
 	SyLittleEndianUnpack16(&nComment,&zCentral[32],sizeof(sxu16));
 	/* Compression method 0 == stored / 8 == deflated */
 	rc = SyLittleEndianUnpack16(&pEntry->nComprMeth,&zCentral[10],sizeof(sxu16));
 	/* DOS Timestamp */
 	SyLittleEndianUnpack16(&nDosTime,&zCentral[12],sizeof(sxu16));
 	SyLittleEndianUnpack16(&nDosDate,&zCentral[14],sizeof(sxu16));
 	SyDosTimeFormat((nDosDate << 16 | nDosTime),&pEntry->sFmt);
	/* Little hack to fix month index  */
	pEntry->sFmt.tm_mon--;
 	/* CRC32 */
 	rc = SyLittleEndianUnpack32(&pEntry->nCrc,&zCentral[16],sizeof(sxu32));
 	/* Content size before compression */
 	rc = SyLittleEndianUnpack32(&pEntry->nByte,&zCentral[24],sizeof(sxu32));
 	if(  pEntry->nByte > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
 	/*
 	 * Content size after compression.
 	 * Note that if the file is stored pEntry->nByte should be equal to pEntry->nByteCompr
 	 */
 	rc = SyLittleEndianUnpack32(&pEntry->nByteCompr,&zCentral[20],sizeof(sxu32));
 	if( pEntry->nByteCompr > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
 	/* Finally grab the contents offset */
 	SyLittleEndianUnpack32(&pEntry->nOfft,&zCentral[42],sizeof(sxu32));
 	if( pEntry->nOfft > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
  	 rc = SXRET_OK;
update:
 	/* Update the offset to point to the next central directory record */
 	*pNextOffset =  SXZIP_CENTRAL_HDRSZ + pName->nByte + pEntry->nExtra + nComment;
 	return rc; /* Report failure or success */
}
static sxi32 ZipFixOffset(SyArchiveEntry *pEntry,void *pSrc)
{
	sxu16 nExtra,nNameLen;
	unsigned char *zHdr;
	nExtra = nNameLen = 0;
	zHdr = (unsigned char *)pSrc;
	zHdr = &zHdr[pEntry->nOfft];
	if( SyMemcmp(zHdr,"PK\003\004",sizeof(sxu32)) != 0 ){
		return SXERR_CORRUPT;
	}
	SyLittleEndianUnpack16(&nNameLen,&zHdr[26],sizeof(sxu16));
	SyLittleEndianUnpack16(&nExtra,&zHdr[28],sizeof(sxu16));
	/* Fix contents offset */
	pEntry->nOfft += SXZIP_LOCAL_HDRSZ + nExtra + nNameLen;
	return SXRET_OK;
}
/*
 * Extract all valid entries from the central directory
 */
static sxi32 ZipExtract(SyArchive *pArch,const unsigned char *zCentral,sxu32 nLen,void *pSrc)
{
	SyArchiveEntry *pEntry,*pDup;
	const unsigned char *zEnd ; /* End of central directory */
	sxu32 nIncr,nOfft;          /* Central Offset */
	SyString *pName;	        /* Entry name */
	char *zName;
	sxi32 rc;

	nOfft = nIncr = 0;
	zEnd = &zCentral[nLen];

	for(;;){
		if( &zCentral[nOfft] >= zEnd ){
			break;
		}
		/* Add a new entry */
		pEntry = (SyArchiveEntry *)SyMemBackendPoolAlloc(pArch->pAllocator,sizeof(SyArchiveEntry));
		if( pEntry == 0 ){
			break;
		}
		SyZero(pEntry,sizeof(SyArchiveEntry));
		pEntry->nMagic = SXARCH_MAGIC;
		nIncr = 0;
		rc = GetCentralDirectoryEntry(&(*pArch),pEntry,&zCentral[nOfft],&nIncr);
		if( rc == SXRET_OK ){
			/* Fix the starting record offset so we can access entry contents correctly */
			rc = ZipFixOffset(pEntry,pSrc);
		}
		if(rc != SXRET_OK ){
			sxu32 nJmp = 0;
			SyMemBackendPoolFree(pArch->pAllocator,pEntry);
			/* Try to recover by brute-forcing for a valid central directory record */
			if( SXRET_OK == SyBlobSearch((const void *)&zCentral[nOfft + nIncr],(sxu32)(zEnd - &zCentral[nOfft + nIncr]),
				(const void *)"PK\001\002",sizeof(sxu32),&nJmp)){
					nOfft += nIncr + nJmp; /* Check next entry */
					continue;
			}
			break; /* Giving up,archive is hopelessly corrupted */
		}
		pName = &pEntry->sFileName;
		pName->zString = (const char *)&zCentral[nOfft + SXZIP_CENTRAL_HDRSZ];
		if( pName->nByte <= 0 || ( pEntry->nByte <= 0 && pName->zString[pName->nByte - 1] != '/') ){
			/* Ignore zero length records (except folders) and records without names */
			SyMemBackendPoolFree(pArch->pAllocator,pEntry);
		 	nOfft += nIncr; /* Check next entry */
			continue;
		}
		zName = SyMemBackendStrDup(pArch->pAllocator,pName->zString,pName->nByte);
 	 	if( zName == 0 ){
 	 		 SyMemBackendPoolFree(pArch->pAllocator,pEntry);
		 	 nOfft += nIncr; /* Check next entry */
			continue;
 	 	}
		pName->zString = (const char *)zName;
		/* Check for duplicates */
		rc = ArchiveHashGetEntry(&(*pArch),pName->zString,pName->nByte,&pDup);
		if( rc == SXRET_OK ){
			/* Another entry with the same name exists ; link them together */
			pEntry->pNextName = pDup->pNextName;
			pDup->pNextName = pEntry;
			pDup->nDup++;
		}else{
			/* Insert in hashtable */
			ArchiveHashInstallEntry(pArch,pEntry);
		}
		nOfft += nIncr;	/* Check next record */
	}
	pArch->pCursor = pArch->pList;

	return pArch->nLoaded > 0 ? SXRET_OK : SXERR_EMPTY;
}
PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen)
 {
 	const unsigned char *zCentral,*zEnd;
 	sxi32 rc;
#if defined(UNTRUST)
 	if( SXARCH_INVALID(pArch) || zBuf == 0 ){
 		return SXERR_INVALID;
 	}
#endif
 	/* The miminal size of a zip archive:
 	 * LOCAL_HDR_SZ + CENTRAL_HDR_SZ + END_OF_CENTRAL_HDR_SZ
 	 * 		30				46				22
 	 */
 	 if( nLen < SXZIP_LOCAL_HDRSZ + SXZIP_CENTRAL_HDRSZ + SXZIP_END_CENTRAL_HDRSZ ){
 	 	return SXERR_CORRUPT; /* Don't bother processing return immediately */
 	 }

 	zEnd = (unsigned char *)&zBuf[nLen - SXZIP_END_CENTRAL_HDRSZ];
 	/* Find the end of central directory */
 	while( ((sxu32)((unsigned char *)&zBuf[nLen] - zEnd) < (SXZIP_END_CENTRAL_HDRSZ + SXI16_HIGH)) &&
		zEnd > (unsigned char *)zBuf && SyMemcmp(zEnd,"PK\005\006",sizeof(sxu32)) != 0 ){
 		zEnd--;
 	}
 	/* Parse the end of central directory */
 	rc = ParseEndOfCentralDirectory(&(*pArch),zEnd);
 	if( rc != SXRET_OK ){
 		return rc;
 	}

 	/* Find the starting offset of the central directory */
 	zCentral = &zEnd[-(sxi32)pArch->nCentralSize];
 	if( zCentral <= (unsigned char *)zBuf || SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){
 		if( pArch->nCentralOfft >= nLen ){
			/* Corrupted central directory offset */
 			return SXERR_CORRUPT;
 		}
 		zCentral = (unsigned char *)&zBuf[pArch->nCentralOfft];
 		if( SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){
 			/* Corrupted zip archive */
 			return SXERR_CORRUPT;
 		}
 		/* Fall thru and extract all valid entries from the central directory */
 	}
 	rc = ZipExtract(&(*pArch),zCentral,(sxu32)(zEnd - zCentral),(void *)zBuf);
 	return rc;
 }
/*
  * Default comparison function.
  */
 static sxi32 ArchiveHashCmp(const SyString *pStr1,const SyString *pStr2)
 {
	 sxi32 rc;
	 rc = SyStringCmp(pStr1,pStr2,SyMemcmp);
	 return rc;
 }
PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp)
 {
	SyArchiveEntry **apHash;
#if defined(UNTRUST)
 	if( pArch == 0 ){
 		return SXERR_EMPTY;
 	}
#endif
 	SyZero(pArch,sizeof(SyArchive));
 	/* Allocate a new hashtable */
	apHash = (SyArchiveEntry **)SyMemBackendAlloc(&(*pAllocator),SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));
	if( apHash == 0){
		return SXERR_MEM;
	}
	SyZero(apHash,SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));
	pArch->apHash = apHash;
	pArch->xHash  = xHash ? xHash : SyBinHash;
	pArch->xCmp   = xCmp ? xCmp : ArchiveHashCmp;
	pArch->nSize  = SXARCHIVE_HASH_SIZE;
 	pArch->pAllocator = &(*pAllocator);
 	pArch->nMagic = SXARCH_MAGIC;
 	return SXRET_OK;
 }
 static sxi32 ArchiveReleaseEntry(SyMemBackend *pAllocator,SyArchiveEntry *pEntry)
 {
 	SyArchiveEntry *pDup = pEntry->pNextName;
 	SyArchiveEntry *pNextDup;

 	/* Release duplicates first since there are not stored in the hashtable */
 	for(;;){
 		if( pEntry->nDup == 0 ){
 			break;
 		}
 		pNextDup = pDup->pNextName;
		pDup->nMagic = 0x2661;
 		SyMemBackendFree(pAllocator,(void *)SyStringData(&pDup->sFileName));
 		SyMemBackendPoolFree(pAllocator,pDup);
 		pDup = pNextDup;
 		pEntry->nDup--;
 	}
	pEntry->nMagic = 0x2661;
  	SyMemBackendFree(pAllocator,(void *)SyStringData(&pEntry->sFileName));
 	SyMemBackendPoolFree(pAllocator,pEntry);
 	return SXRET_OK;
 }
PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch)
 {
	SyArchiveEntry *pEntry,*pNext;
 	pEntry = pArch->pList;
	for(;;){
		if( pArch->nLoaded < 1 ){
			break;
		}
		pNext = pEntry->pNext;
		MACRO_LD_REMOVE(pArch->pList,pEntry);
		ArchiveReleaseEntry(pArch->pAllocator,pEntry);
		pEntry = pNext;
		pArch->nLoaded--;
	}
	SyMemBackendFree(pArch->pAllocator,pArch->apHash);
	pArch->pCursor = 0;
	pArch->nMagic = 0x2626;
	return SXRET_OK;
 }
 PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch)
 {
	pArch->pCursor = pArch->pList;
	return SXRET_OK;
 }
 PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry)
 {
	SyArchiveEntry *pNext;
	if( pArch->pCursor == 0 ){
		/* Rewind the cursor */
		pArch->pCursor = pArch->pList;
		return SXERR_EOF;
	}
	*ppEntry = pArch->pCursor;
	 pNext = pArch->pCursor->pNext;
	 /* Advance the cursor to the next entry */
	 pArch->pCursor = pNext;
	 return SXRET_OK;
  }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Psuedo Random Number Generator (PRNG)
 * @authors: SQLite authors <http://www.sqlite.org/>
 * @status: Public Domain
 * NOTE:
 *  Nothing in this file or anywhere else in the library does any kind of
 *  encryption.The RC4 algorithm is being used as a PRNG (pseudo-random
 *  number generator) not as an encryption device.
 */
#define SXPRNG_MAGIC	0x13C4
#ifdef __WINNT__
#include <windows.h>
#endif
#ifdef __UNIXES__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#endif
static sxi32 SyOSUtilRandomSeed(void *pBuf,sxu32 nLen,void *pUnused)
{
	char *zBuf = (char *)pBuf;
#ifdef __WINNT__
	DWORD nProcessID; /* Yes,keep it uninitialized when compiling using the MinGW32 builds tools */
#elif defined(__UNIXES__)
	pid_t pid;
	int fd;
#else
	char zGarbage[128]; /* Yes,keep this buffer uninitialized */
#endif
	SXUNUSED(pUnused);
#ifdef __WINNT__
#ifndef __MINGW32__
	nProcessID = GetProcessId(GetCurrentProcess());
#endif
	SyMemcpy((const void *)&nProcessID,zBuf,SXMIN(nLen,sizeof(DWORD)));
	if( (sxu32)(&zBuf[nLen] - &zBuf[sizeof(DWORD)]) >= sizeof(SYSTEMTIME)  ){
		GetSystemTime((LPSYSTEMTIME)&zBuf[sizeof(DWORD)]);
	}
#elif defined(__UNIXES__)
	fd = open("/dev/urandom",O_RDONLY);
	if (fd >= 0 ){
		if( read(fd,zBuf,nLen) > 0 ){
			close(fd);
			return SXRET_OK;
		}
		/* FALL THRU */
	}
	close(fd);
	pid = getpid();
	SyMemcpy((const void *)&pid,zBuf,SXMIN(nLen,sizeof(pid_t)));
	if( &zBuf[nLen] - &zBuf[sizeof(pid_t)] >= (int)sizeof(struct timeval)  ){
		gettimeofday((struct timeval *)&zBuf[sizeof(pid_t)],0);
	}
#else
	/* Fill with uninitialized data */
	SyMemcpy(zGarbage,zBuf,SXMIN(nLen,sizeof(zGarbage)));
#endif
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void * pUserData)
{
	char zSeed[256];
	sxu8 t;
	sxi32 rc;
	sxu32 i;
	if( pCtx->nMagic == SXPRNG_MAGIC ){
		return SXRET_OK; /* Already initialized */
	}
 /* Initialize the state of the random number generator once,
  ** the first time this routine is called.The seed value does
  ** not need to contain a lot of randomness since we are not
  ** trying to do secure encryption or anything like that...
  */
	if( xSeed == 0 ){
		xSeed = SyOSUtilRandomSeed;
	}
	rc = xSeed(zSeed,sizeof(zSeed),pUserData);
	if( rc != SXRET_OK ){
		return rc;
	}
	pCtx->i = pCtx->j = 0;
	for(i=0; i < SX_ARRAYSIZE(pCtx->s) ; i++){
		pCtx->s[i] = (unsigned char)i;
    }
    for(i=0; i < sizeof(zSeed) ; i++){
      pCtx->j += pCtx->s[i] + zSeed[i];
      t = pCtx->s[pCtx->j];
      pCtx->s[pCtx->j] = pCtx->s[i];
      pCtx->s[i] = t;
    }
	pCtx->nMagic = SXPRNG_MAGIC;

	return SXRET_OK;
}
/*
 * Get a single 8-bit random value using the RC4 PRNG.
 */
static sxu8 randomByte(SyPRNGCtx *pCtx)
{
  sxu8 t;

  /* Generate and return single random byte */
  pCtx->i++;
  t = pCtx->s[pCtx->i];
  pCtx->j += t;
  pCtx->s[pCtx->i] = pCtx->s[pCtx->j];
  pCtx->s[pCtx->j] = t;
  t += pCtx->s[pCtx->i];
  return pCtx->s[t];
}
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen)
{
	unsigned char *zBuf = (unsigned char *)pBuf;
	unsigned char *zEnd = &zBuf[nLen];
#if defined(UNTRUST)
	if( pCtx == 0 || pBuf == 0 || nLen <= 0 ){
		return SXERR_EMPTY;
	}
#endif
	if(pCtx->nMagic != SXPRNG_MAGIC ){
		return SXERR_CORRUPT;
	}
	for(;;){
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
		if( zBuf >= zEnd ){break;}	zBuf[0] = randomByte(pCtx);	zBuf++;
	}
	return SXRET_OK;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_HASH_FUNC
#endif /* PH7_DISABLE_HASH_FUNC */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
