/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>   /* snprintf (printf-family float conversions — correctly
                      * rounded shortest-representation output) */
/*
 * Section:
 *    printf-style format engine and the sprintf/printf function family.
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_DISK_IO
#define PH7_NEED_FMT_AND_INI 1
#endif
#ifdef PH7_NEED_FMT_AND_INI
#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define PH7_FMT_FLOAT       2 /* Floating point.%f */
#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */
#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */
#define PH7_FMT_STRING      6 /* Strings.%s */
#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */
#define PH7_FMT_CHARX       8 /* Characters.%c */
#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */

/*
** Allowed values for ph7_fmt_info.flags
*/
#define PH7_FMT_FLAG_SIGNED	  0x01
#define PH7_FMT_FLAG_UNSIGNED 0x02
/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct ph7_fmt_info ph7_fmt_info;
struct ph7_fmt_info
{
  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */
  sxu8 base;     /* The base for radix conversion */
  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */
  sxu8 type;     /* Conversion paradigm */
  char *charset; /* The character set for conversion */
  char *prefix;  /* Prefix on non-zero values in alt format */
};
/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —
 * the default float->string cast needs it even when this whole formatting
 * region is compiled out by PH7_DISABLE_DISK_IO. */
/*
 * The following table is searched linearly, so it is good to put the most frequently
 * used conversion types first.
 */
static const ph7_fmt_info aFmt[] = {
  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },
  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },
  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },
  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },
  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },
  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},
  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },
  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },
  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },
  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },
  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },
  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },
  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always
   * formats in the C locale, so they behave identically. */
  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },
  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },
  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }
};
/*
 * PHP 8 raises a catchable ValueError for an unknown conversion specifier
 * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()
 * and fprintf() stream their output incrementally while sprintf() buffers it,
 * every format builtin calls PH7_FormatValidate (below) to check the whole
 * format string BEFORE formatting so the throw happens with no partial output
 * escaping (php buffers the entire result and only emits it on success). This
 * scan mirrors the specifier-locating logic of the main format loop below.
 * On the first unknown specifier, stores it in *pBad and returns TRUE; returns
 * FALSE when every specifier is known. (A found-flag rather than a sentinel
 * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for
 * "all valid".)
 */
static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad,int *pbDangling)
{
	const char *zEnd = &zIn[nByte];
	int c,idx;
	while( zIn < zEnd ){
		if( zIn[0] != '%' ){
			zIn++;
			continue;
		}
		zIn++; /* jump the percent sign */
		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad
		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an
		 * unknown specifier, matching php. */
		while( zIn < zEnd ){
			c = zIn[0];
			if( c=='-' || c=='+' || c==' ' || c=='0' ){
				zIn++;
				continue;
			}
			if( c=='\'' ){
				zIn++;
				if( zIn < zEnd ){
					zIn++; /* the custom pad character */
				}
				continue;
			}
			break;
		}
		/* field width */
		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
			zIn++;
		}
		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),
		 * so skip the full flag set and width again, mirroring the main loop. */
		if( zIn < zEnd && zIn[0]=='$' ){
			zIn++;
			while( zIn < zEnd ){
				c = zIn[0];
				if( c=='-' || c=='+' || c==' ' || c=='0' ){
					zIn++;
					continue;
				}
				if( c=='\'' ){
					zIn++;
					if( zIn < zEnd ){
						zIn++;
					}
					continue;
				}
				break;
			}
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
				zIn++;
			}
		}
		/* precision */
		if( zIn < zEnd && zIn[0]=='.' ){
			zIn++;
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
				zIn++;
			}
		}
		/* a single 'l' length modifier (ignored, php compat) */
		if( zIn < zEnd && zIn[0]=='l' ){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* A dangling '%' the format string ends on: php raises
			 * `ValueError: Missing format specifier at end of string`. */
			*pbDangling = TRUE;
			return FALSE;
		}
		c = zIn[0];
		zIn++; /* jump the conversion specifier */
		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){
			if( c == aFmt[idx].fmttype ){
				break;
			}
		}
		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){
			*pBad = c; /* unknown specifier */
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Validate a printf-style format string. PHP 8 raises a catchable ValueError for
 * an unknown conversion specifier, thrown before any output is produced. Every
 * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this
 * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the
 * throw is caught in place, PH7_ABORT when it goes uncaught).
 * Returns PH7_OK when the format is valid.
 */
PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)
{
	int badSpec = 0,bDangling = FALSE;
	if( FormatUnknownSpec(zFormat,nByte,&badSpec,&bDangling) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Unknown format specifier \"%c\"",badSpec);
	}
	if( bDangling ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Missing format specifier at end of string");
	}
	return PH7_OK;
}
/*
 * Count the number of VALUE arguments a format string needs: the greater of the
 * sequential (non-positional) conversion count and the highest positional index
 * (`%N$`). `%%` consumes nothing. Mirrors FormatUnknownSpec's specifier walk.
 */
static int FormatRequiredArgs(const char *zIn,int nByte)
{
	const char *zEnd = &zIn[nByte];
	int c,seq = 0,maxpos = 0;
	while( zIn < zEnd ){
		int numVal = 0,pos = 0;
		if( zIn[0] != '%' ){
			zIn++;
			continue;
		}
		zIn++; /* jump the percent sign */
		/* leading flags (incl. the "'<pad>'" custom-pad form) */
		while( zIn < zEnd ){
			c = zIn[0];
			if( c=='-' || c=='+' || c==' ' || c=='0' ){ zIn++; continue; }
			if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }
			break;
		}
		/* leading number: a positional index when a '$' follows, else the width */
		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
			numVal = numVal*10 + (zIn[0]-'0');
			zIn++;
		}
		if( zIn < zEnd && zIn[0]=='$' ){
			pos = numVal;
			zIn++;
			/* flags then width may follow the positional marker */
			while( zIn < zEnd ){
				c = zIn[0];
				if( c=='-' || c=='+' || c==' ' || c=='0' ){ zIn++; continue; }
				if( c=='\'' ){ zIn++; if( zIn < zEnd ){ zIn++; } continue; }
				break;
			}
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }
		}
		/* precision */
		if( zIn < zEnd && zIn[0]=='.' ){
			zIn++;
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){ zIn++; }
		}
		/* a single 'l' length modifier (ignored, php compat) */
		if( zIn < zEnd && zIn[0]=='l' ){ zIn++; }
		if( zIn >= zEnd ){
			/* A dangling '%' still COUNTS as needing a value: php reports
			 * sprintf("%") as "2 arguments are required, 1 given" and only
			 * raises the missing-specifier ValueError once the count is met. */
			if( pos > 0 ){
				if( pos > maxpos ){ maxpos = pos; }
			}else{
				seq++;
			}
			break;
		}
		c = zIn[0];
		zIn++; /* jump the conversion specifier */
		if( c == '%' ){ continue; } /* %% consumes no argument */
		if( pos > 0 ){
			if( pos > maxpos ){ maxpos = pos; }
		}else{
			seq++;
		}
	}
	return seq > maxpos ? seq : maxpos;
}
/*
 * PHP 8: a printf-family call with fewer VALUE arguments than the format needs
 * throws BEFORE any output. The non-vararg family (sprintf/printf/fprintf) raises
 * ArgumentCountError counting the format itself ("N arguments are required, M
 * given"); the vararg family (vsprintf/vprintf/vfprintf) raises a ValueError over
 * the values array ("The arguments array must contain N items, M given"). nValues
 * is the count of value arguments actually supplied; nFixed is the number of
 * fixed leading parameters counted in the ArgumentCountError totals (1 for the
 * $format of sprintf/printf, 2 for fprintf's $stream + $format — the vararg
 * ValueError counts only the array, so nFixed is ignored there). Returns PH7_OK
 * when enough.
 */
PH7_PRIVATE sxi32 PH7_FormatCheckArgCount(ph7_context *pCtx,const char *zFormat,int nByte,int nValues,int nFixed,int bVararg)
{
	int required = FormatRequiredArgs(zFormat,nByte);
	if( nValues < required ){
		if( bVararg ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"The arguments array must contain %d items, %d given",required,nValues);
		}
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"%d arguments are required, %d given",required+nFixed,nValues+nFixed);
	}
	return PH7_OK;
}
/*
 * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars
 * (int/float/bool) and null coerce to a string, but an array/object/resource
 * raises a catchable TypeError. iArg is the 1-based argument position ($format
 * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns
 * PH7_OK when the value is string-coercible (the caller then uses
 * ph7_value_to_string, which renders scalars/null verbatim).
 */
/*
 * php 8: a stream parameter that is not a resource is a TypeError, not a warning
 * with a 0 return -- the caller never learned its write went nowhere.
 */
PH7_PRIVATE sxi32 PH7_CheckStreamArg(ph7_context *pCtx,ph7_value *pArg,int iArg,const char *zName)
{
	if( !ph7_value_is_resource(pArg) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($%s) must be of type resource, %s given",
			ph7_function_name(pCtx),iArg,zName,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));
	}
	return PH7_OK;
}
PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)
{
	if( ph7_value_is_array(pArg) || ph7_value_is_object(pArg) || ph7_value_is_resource(pArg) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($format) must be of type string, %s given",
			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));
	}
	return PH7_OK;
}
/*
 * Format a given string.
 * The root program.  All variations call this core.
 * INPUTS:
 *   xConsumer   This is a pointer to a function taking four arguments
 *            1. A pointer to the call context.
 *            2. A pointer to the list of characters to be output
 *               (Note, this list is NOT null terminated.)
 *            3. An integer number of characters to be output.
 *               (Note: This number might be zero.)
 *            4. Upper layer private data.
 *   zIn       This is the format string, as in the usual print.
 *   apArg     This is a pointer to a list of arguments.
 */
PH7_PRIVATE sxi32 PH7_InputFormat(
	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */
	ph7_context *pCtx,  /* call context */
	const char *zIn,    /* Format string */
	int nByte,          /* Format string length */
	int nArg,           /* Total argument of the given arguments */
	ph7_value **apArg,  /* User arguments */
	void *pUserData,    /* Last argument to xConsumer() */
	int vf              /* TRUE if called from vfprintf,vsprintf context */
	)
{
	char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
	const char *zCur,*zEnd = &zIn[nByte];
	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */
	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */
	int flag_alternateform; /* True if "#" flag is present */
	int flag_leftjustify;   /* True if "-" flag is present */
	int flag_blanksign;     /* True if " " flag is present */
	int flag_plussign;      /* True if "+" flag is present */
	int flag_zeropad;       /* True if field width constant starts with zero */
	ph7_value *pArg;         /* Current processed argument */
	ph7_int64 iVal;
	int precision;           /* Precision of the current field */
	/* zExtra (unused) removed to prevent compiler warning. */
	int c,rc,n;
	ph7_value *pThrowArg = 0; /* First not-stringable %s argument; throws at the end */
	int length;              /* Length of the field */
	int prefix;
	sxu8 xtype;              /* Conversion paradigm */
	int width;               /* Width of the current field */
	int idx;
	n = (vf == TRUE) ? 0 : 1;
#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )
	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()
	 * (called by every format builtin before this routine), so the specifier set
	 * seen here is always valid. */
	/* Start the format process */
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '%' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Consume chunk verbatim */
			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);
			if( rc != SXRET_OK ){
				/* Callback requested an abort (e.g. an allocation failure) */
				break;
			}
		}
		if( zIn >= zEnd ){
			/* No more input to process,break immediately */
			break;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_plussign = flag_blanksign =
			flag_alternateform = flag_zeropad = 0;
		/* Reset the pad buffer to spaces: a custom pad char ('X) — or the string
		 * zero-pad below — from a PREVIOUS specifier must not bleed into this one.
		 * php resets the pad character for every specifier. */
		for( idx = 0 ; idx < etSPACESIZE ; ++idx ){ spaces[idx] = ' '; }
		zIn++; /* Jump the precent sign */
		do{
			c = zIn[0];
			switch( c ){
			case '-':   flag_leftjustify = 1;     c = 0;   break;
			case '+':   flag_plussign = 1;        c = 0;   break;
			case ' ':   flag_blanksign = 1;       c = 0;   break;
			case '0':   flag_zeropad = 1;         c = 0;   break;
			case '\'':
				zIn++;
				if( zIn < zEnd ){
					/* An alternate padding character can be specified by prefixing it with a single quote (') */
					c = zIn[0];
					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
						spaces[idx] = (char)c;
					}
					c = 0;
				}
				break;
			default:                                       break;
			}
		}while( c==0 && (zIn++ < zEnd) );
		/* Get the field width */
		width = 0;
		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
			width = width*10 + (zIn[0] - '0');
			zIn++;
		}
		if( zIn < zEnd && zIn[0] == '$' ){
			/* Position specifer */
			if( width > 0 ){
				n = width;
				if( vf && n > 0 ){
					n--;
				}
			}
			zIn++;
			width = 0;
			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the
			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),
			 * not just zero-padding. */
			do{
				c = zIn[0];
				switch( c ){
				case '-':   flag_leftjustify = 1;     c = 0;   break;
				case '+':   flag_plussign = 1;        c = 0;   break;
				case ' ':   flag_blanksign = 1;       c = 0;   break;
				case '0':   flag_zeropad = 1;         c = 0;   break;
				case '\'':
					zIn++;
					if( zIn < zEnd ){
						c = zIn[0];
						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
							spaces[idx] = (char)c;
						}
						c = 0;
					}
					break;
				default:                                       break;
				}
			}while( c==0 && (zIn++ < zEnd) );
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				width = width*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		if( width > PH7_FMT_BUFSIZ-10 ){
			width = PH7_FMT_BUFSIZ-10;
		}
		/* Get the precision */
		precision = -1;
		if( zIn < zEnd && zIn[0] == '.' ){
			precision = 0;
			zIn++;
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				precision = precision*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,
		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:
		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */
		if( zIn < zEnd && zIn[0] == 'l' ){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		/* Fetch the info entry for the field */
		pInfo = 0;
		xtype = PH7_FMT_ERROR;
		c = zIn[0];
		zIn++; /* Jump the format specifer */
		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){
			if( c==aFmt[idx].fmttype ){
				pInfo = &aFmt[idx];
				xtype = pInfo->type;
				break;
			}
		}
		zBuf = zWorker; /* Point to the working buffer */
		length = 0;
		/* zExtra previously assigned here; not used anywhere, removed. */
		 /*
		  ** At this point, variables are initialized as follows:
		  **
		  **   flag_alternateform          TRUE if a '#' is present.
		  **   flag_plussign               TRUE if a '+' is present.
		  **   flag_leftjustify            TRUE if a '-' is present or if the
		  **                               field width was negative.
		  **   flag_zeropad                TRUE if the width began with 0.
		  **                               the conversion character.
		  **   flag_blanksign              TRUE if a ' ' is present.
		  **   width                       The specified field width.  This is
		  **                               always non-negative.  Zero is the default.
		  **   precision                   The specified precision.  The default
		  **                               is -1.
		  */
		switch(xtype){
		case PH7_FMT_PERCENT:
			/* A literal percent character */
			zWorker[0] = '%';
			length = (int)sizeof(char);
			break;
		case PH7_FMT_CHARX:
			/* The argument is treated as an integer, and presented as the character
			 * with that ASCII value
			 */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				c = 0;
			}else{
				c = ph7_value_to_int(pArg);
			}
			/* NUL byte is an acceptable value */
			zWorker[0] = (char)c;
			length = (int)sizeof(char);
			break;
		case PH7_FMT_STRING:
			/* the argument is treated as and presented as a string */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				length = 0;
			}else if( PH7_MemObjIsNotStringable(pArg) ){
				/* php's user-visible coercion for %s (§2), object half: a class with
				 * no __toString() is the catchable "could not be converted to
				 * string" Error — but php does NOT let it interrupt the format. The
				 * conversion substitutes NOTHING, the format runs to the end, the
				 * output is written, and only then does the Error surface. So the
				 * throw cannot be RAISED here: PHL's VmThrowException runs an
				 * in-place catch immediately, which would print the format's tail
				 * after the catch body. Remember the value and throw once the
				 * output is out (see the tail of this function). */
				zBuf = "";
				length = 0;
				if( pThrowArg == 0 ){
					pThrowArg = pArg;
				}
			}else{
				/* An ARRAY warns and renders as "Array"; a Stringable renders. */
				const char *zSv;
				sxi32 rcSv = PH7_ValueToStringUV(pCtx,pArg,&zSv,&length);
				zBuf = (char *)zSv;
				if( rcSv != SXRET_OK ){
					/* A __toString() that THREW: unlike the case above this one
					 * cannot be predicted, and the throw has already run any
					 * in-place catch. Stop formatting rather than emitting the
					 * format's tail after the catch body — every other builtin that
					 * calls user code (array_map, usort) stops the same way. php
					 * keeps going and prints the tail; recorded divergence, and
					 * both engines raise the same exception. */
					return rcSv;
				}
			}
			if( length < 1 ){
				/* An empty %s substitutes NOTHING in php. PH7 substituted a single
				 * SPACE here, so printf("[%s]","") printed "[ ]" and any format with an
				 * absent optional part gained a stray space. */
				zBuf = "";
				length = 0;
			}
			if( precision>=0 && precision<length ){
				length = precision;
			}
			if( flag_zeropad ){
				/* zero-padding works on strings too */
				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
					spaces[idx] = '0';
				}
			}
			break;
		case PH7_FMT_RADIX:
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				iVal = 0;
			}else{
				iVal = ph7_value_to_int64(pArg);
			}
			/* Limit the precision to prevent overflowing buf[] during conversion */
			if( precision>PH7_FMT_BUFSIZ-40 ){
				precision = PH7_FMT_BUFSIZ-40;
			}
#if 1
        /* For the format %#x, the value zero is printed "0" not "0x0".
        ** I think this is stupid.*/
        if( iVal==0 ) flag_alternateform = 0;
#else
        /* More sensible: turn off the prefix for octal (to prevent "00"),
        ** but leave the prefix for hex.*/
        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;
#endif
        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){
          if( iVal<0 ){
            iVal = -iVal;
			/* Ticket 1433-003 */
			if( iVal < 0 ){
				/* Overflow */
				iVal= 0x7FFFFFFFFFFFFFFF;
			}
            prefix = '-';
          }else if( flag_plussign )  prefix = '+';
          else if( flag_blanksign )  prefix = ' ';
          else                       prefix = 0;
        }else{
			if( iVal<0 ){
				iVal = -iVal;
				/* Ticket 1433-003 */
				if( iVal < 0 ){
					/* Overflow */
					iVal= 0x7FFFFFFFFFFFFFFF;
				}
			}
			prefix = 0;
		}
        if( flag_zeropad && precision<width-(prefix!=0) ){
          precision = width-(prefix!=0);
        }
        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];
        {
          register char *cset;      /* Use registers for speed */
          register int base;
          cset = pInfo->charset;
          base = pInfo->base;
          do{                                           /* Convert to ascii */
            *(--zBuf) = cset[iVal%base];
            iVal = iVal/base;
          }while( iVal>0 );
        }
		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);
        for(idx=precision-length; idx>0; idx--){
          *(--zBuf) = '0';                             /* Zero pad */
        }
        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */
        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */
          char *pre, x;
          pre = pInfo->prefix;
          if( *zBuf!=pre[0] ){
            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;
          }
        }
		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);
		break;
		case PH7_FMT_FLOAT:
		case PH7_FMT_EXP:
		case PH7_FMT_GENERIC:{
#ifndef PH7_OMIT_FLOATING_POINT
		double realvalue;
		char zFmt[8];
		int nOut, nFmt;
		pArg = NEXT_ARG;
		if( pArg == 0 ){
			realvalue = 0;
		}else{
			realvalue = ph7_value_to_double(pArg);
		}
		/* php prints the IEEE specials bare — NaN / INF / -INF with no width
		 * padding, precision, or sign flags (php_sprintf_appenddouble). */
		if( PH7_IS_NAN(realvalue) ){
			zBuf = "NaN";
			length = 3;
			width = 0;
			break;
		}
		if( PH7_IS_INF(realvalue) ){
			if( realvalue < 0.0 ){
				zBuf = "-INF";
				length = 4;
			}else{
				zBuf = "INF";
				length = 3;
			}
			width = 0;
			break;
		}
		if( precision<0 ) precision = 6;         /* Set default precision */
		if( precision > 53 ){
			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE
			 * (message prefixed with the active function's name, like
			 * php_error_docref). */
			char zMsg[160];
			SyBufferFormat(zMsg,sizeof(zMsg),
				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",
				&pCtx->pFunc->sName,precision,53);
			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);
			precision = 53;
		}
		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints
		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */
		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){
			realvalue = 0.0;
		}
		/* php's float conversions are correctly rounded (zend_dtoa); use libc
		 * snprintf as the digit engine (the byte-exact-floats rule — the old
		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so
		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64
		 * expansion), then post-process into php's exact shapes below. */
		nFmt = 0;
		zFmt[nFmt++] = '%';
		if( flag_alternateform ) zFmt[nFmt++] = '#';
		/* php's ' ' flag selects space PADDING (its default), not C's
		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */
		if( flag_plussign ) zFmt[nFmt++] = '+';
		zFmt[nFmt++] = '.';
		zFmt[nFmt++] = '*';
		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :
			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')
			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));
		zFmt[nFmt] = 0;
		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);
		if( nOut < 0 || nOut >= (int)sizeof(zWorker) ){
			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is
			 * ~365 bytes); keep the truncated output rather than overrun. */
			nOut = (int)SyStrlen(zWorker);
		}
		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);
		zBuf = zWorker;
		length = nOut;
		/* Let the zero-pad block below insert zeros between the sign (written
		 * by snprintf) and the first digit, as before. */
		prefix = (zWorker[0]=='-' || zWorker[0]=='+' || zWorker[0]==' ') ? zWorker[0] : 0;
        /* Special case:  Add leading zeros if the flag_zeropad flag is
        ** set and we are not left justified */
        if( flag_zeropad && !flag_leftjustify && length < width){
          int i;
          int nPad = width - length;
          for(i=width; i>=nPad; i--){
            zBuf[i] = zBuf[i-nPad];
          }
          i = prefix!=0;
          while( nPad-- ) zBuf[i++] = '0';
          length = width;
        }
#else
         zBuf = " ";
		 length = (int)sizeof(char);
#endif /* PH7_OMIT_FLOATING_POINT */
		 break;
							 }
		default:
			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a
			 * catchable ValueError before formatting begins. Kept as a defensive
			 * no-op that emits nothing. */
			length = 0;
			break;
		}
		 /*
		 ** The text of the conversion is pointed to by "zBuf" and is
		 ** "length" characters long.The field width is "width".Do
		 ** the output.
		 */
    if( !flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
    if( length>0 ){
		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);
		if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
		}
    }
    if( flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
 }/* for(;;) */
	if( pThrowArg ){
		/* The format ran to completion and its output is out; raise php's Error
		 * now. `printf("A[%s]B", new P())` prints "A[]B" and THEN throws, while
		 * sprintf()'s finished result is simply discarded by the unwind. */
		return PH7_MemObjToStringUV(pThrowArg);
	}
	return SXRET_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the sprintf function.
 */
static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	/* pUserData points to the caller's allocation-rc slot so an OOM during the
	 * result append is surfaced (the builtin raises a fatal); returning the
	 * non-OK rc also stops the format loop. */
	sxi32 *pRc = (sxi32 *)pUserData;
	*pRc = ph7_result_string(pCtx,zInput,nLen);
	return *pRc;
}
/*
 * string sprintf(string $format[,mixed $args [, mixed $... ]])
 *  Return a formatted string.
 * Parameters
 *  $format
 *    The format string (see block comment above)
 * Return
 *  A string produced according to the formatting string format.
 */
PH7_PRIVATE int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rcFmt;
	const char *zFormat;
	sxi32 rc = SXRET_OK;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */
	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rc != PH7_OK ){
		return rc;
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8: an unknown format specifier throws a catchable ValueError before any
	 * output; propagate the throw status verbatim. */
	rc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);
	if( rc != PH7_OK ){
		return rc;
	}
	/* PHP 8: too few value arguments is a catchable ArgumentCountError before output. */
	rc = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rc != PH7_OK ){
		return rc;
	}
	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */
	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);
	if( rc != SXRET_OK ){
		/* The result append ran out of memory: raise a fatal rather than
		 * returning a silently-truncated string. */
		return PH7_ContextMemoryError(pCtx);
	}
	/* A %s argument that could not be coerced raised php's Error mid-format. The
	 * format still ran and the output/result still happened (php does exactly
	 * that), so report the throw last. */
	if( rcFmt != SXRET_OK ){
		pCtx->nThrowRc = rcFmt;
		return rcFmt;
	}
	return PH7_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the printf function.
 */
static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	ph7_int64 *pCounter = (ph7_int64 *)pUserData;
	/* Call the VM output consumer directly */
	ph7_context_output(pCtx,zInput,nLen);
	/* Increment counter */
	*pCounter += nLen;
	return PH7_OK;
}
/*
 * int64 printf(string $format[,mixed $args[,mixed $... ]])
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
PH7_PRIVATE int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rcFmt;
	ph7_int64 nCounter = 0;
	const char *zFormat;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */
	{
		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
		if( rcf != PH7_OK ){
			return rcf;
		}
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	{
		/* PHP 8: too few value arguments is a catchable ArgumentCountError before
		 * output, and php runs this check BEFORE validating the specifiers. */
		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg-1,1,FALSE);
		if( rcv != PH7_OK ){
			return rcv;
		}
		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError
		 * before any output; propagate the throw status verbatim. */
		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);
		if( rcv != PH7_OK ){
			return rcv;
		}
	}
	/* Format the string */
	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	/* A %s argument that could not be coerced raised php's Error mid-format. The
	 * format still ran and the output/result still happened (php does exactly
	 * that), so report the throw last. */
	if( rcFmt != SXRET_OK ){
		pCtx->nThrowRc = rcFmt;
		return rcFmt;
	}
	return PH7_OK;
}
/*
 * int vprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
PH7_PRIVATE int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rcFmt;
	ph7_int64 nCounter = 0;
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	int nLen,n;
	if( nArg < 2 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */
	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		/* PHP 8: a non-array $values is a catchable TypeError. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"vprintf(): Argument #2 ($values) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* PHP 8: too few items in the $values array is a catchable ValueError before output.
	 * Checked on the entry count before materialising the value set. php runs this check
	 * BEFORE validating the specifiers, so vsprintf("%",[]) reports the missing item
	 * rather than the missing specifier. */
	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before
	 * any output; propagate the throw status verbatim. */
	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string */
	rcFmt = PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);
	/* Release the container */
	SySetRelease(&sArg);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	/* A %s argument that could not be coerced raised php's Error mid-format. The
	 * format still ran and the output/result still happened (php does exactly
	 * that), so report the throw last. */
	if( rcFmt != SXRET_OK ){
		pCtx->nThrowRc = rcFmt;
		return rcFmt;
	}
	return PH7_OK;
}
/*
 * int vsprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  A string produced according to the formatting string format.
 */
PH7_PRIVATE int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rcFmt;
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	sxi32 rc = SXRET_OK;
	int nLen,n;
	if( nArg < 2 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */
	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rc != PH7_OK ){
		return rc;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		/* PHP 8: a non-array $values is a catchable TypeError. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"vsprintf(): Argument #2 ($values) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* PHP 8: too few items in the $values array is a catchable ValueError before output.
	 * php runs this BEFORE validating the specifiers. */
	rcFmt = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before
	 * any output; propagate the throw status verbatim. */
	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */
	rcFmt = PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);
	/* Release the container */
	SySetRelease(&sArg);
	if( rc != SXRET_OK ){
		/* The result append ran out of memory: raise a fatal. */
		return PH7_ContextMemoryError(pCtx);
	}
	/* A %s argument that could not be coerced raised php's Error mid-format. The
	 * format still ran and the output/result still happened (php does exactly
	 * that), so report the throw last. */
	if( rcFmt != SXRET_OK ){
		pCtx->nThrowRc = rcFmt;
		return rcFmt;
	}
	return PH7_OK;
}
#endif /* PH7_NEED_FMT_AND_INI */
