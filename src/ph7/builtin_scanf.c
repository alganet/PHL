/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h>  /* strtod (the %f/%e/%g conversion) */
/*
 * Section:
 *    Formatted input: sscanf() and the scanner fscanf() shares with it.
 * Status:
 *    Stable.
 *
 * php's scanf is not C's. It came from Tcl 8.3 by way of ext/standard/scanf.c
 * and keeps that lineage's rules, several of which a re-derivation from
 * scanf(3) gets wrong:
 *
 *   - the format is READ TWICE. The first pass validates it and counts the
 *     substitutions, raising every ValueError this family has before a single
 *     byte of input is looked at; the second pass does the scanning. So a
 *     format whose LAST specifier is bad refuses the whole call, and a bad
 *     one is a thrown error rather than a false return.
 *   - `%c` is not C's single character: it is `%s` with the whitespace SKIP
 *     turned off and a default width of one, so it stops at a space. `"  ab"`
 *     read with `%c` answers the EMPTY string, and counts as a conversion.
 *   - `%n$` (XPG3 positional) may not be mixed with plain `%`, a suppressed
 *     `%*d` counts as neither, and with no variables passed the highest index
 *     decides how long the answer array is.
 *   - a conversion the input cannot satisfy stops the scan where it stands and
 *     everything after it stays NULL; only hitting the end of the input with
 *     NOTHING converted is the -1 / NULL "EOF" answer.
 *   - the value is scanned into a 64-byte buffer, so a width past 63 is
 *     clamped, and both the integer and the float scanners BACK UP over a
 *     trailing `0x` / `e` / sign they turned out not to be able to use.
 *
 * Both builtins take the string as a C string, php's own limitation: a NUL
 * byte in either the subject or the format ends it there.
 */
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
/*
 * php caps a positional `%n$` index at 255 when no variables were passed --
 * "in the interest of security and resource friendliness", since the index
 * alone decides how large the answer array is.
 */
#define SCANF_MAX_ARGS 0xFF
/* The scratch a number is accumulated into before it is converted. Its size is
 * what bounds a field WIDTH for the numeric conversions. */
#define SCANF_NUM_BUF   64
/* Per-specifier flags, php's own set. */
#define SCANF_NOSKIP    0x001  /* Do not skip leading whitespace (%c and %[) */
#define SCANF_SUPPRESS  0x002  /* `%*` -- scan the field, assign nothing */
#define SCANF_UNSIGNED  0x004  /* %u -- the value is read as unsigned */
#define SCANF_SIGNOK    0x010  /* a +/- is still allowed here */
#define SCANF_NODIGITS  0x020  /* no digit has been accepted yet */
#define SCANF_NOZERO    0x040  /* no leading zero has been accepted yet */
#define SCANF_XOK       0x080  /* an `x` may follow (a 0 in base 16 / base 0) */
#define SCANF_PTOK      0x100  /* the decimal point is still allowed */
#define SCANF_EXPOK     0x200  /* an exponent is still allowed */
/*
 * A `%[...]` character set: loose characters and ranges kept apart, exactly as
 * php builds them.
 *
 * The comparisons are made on a SIGNED char on purpose, because php's are: its
 * CharSet holds `char` and compares with `<=`, so on the platforms this engine
 * targets a byte at or above 0x80 is NEGATIVE, and that decides both what a
 * range contains and whether php considers it written backwards. `%[\x01-\xff]`
 * is therefore the range 0xff..0x01 read back to front, and matches nothing an
 * ASCII string can offer -- an answer only a differential finds, and one a
 * rewrite over unsigned bytes would quietly "fix".
 */
typedef struct scanf_charset {
	int exclude;              /* `^` -- the set is what is NOT listed */
	int nchars;
	signed char *chars;
	int nranges;
	struct scanf_range {
		signed char start;
		signed char end;
	} *ranges;
} scanf_charset;
/*
 * Where a converted value goes. Both callers hand the same sink in: with
 * variables passed, each conversion writes through one of them and the call
 * answers how many conversions happened; with none, the call answers an ARRAY
 * pre-filled with as many NULLs as the format has substitutions, and a
 * conversion overwrites its own slot.
 */
typedef struct scanf_sink {
	ph7_context *pCtx;
	ph7_value **apVar;   /* the by-reference variadic tail, or 0 */
	int nVar;            /* how many of them */
	ph7_value *pArray;   /* the answer array, when nVar == 0 */
	ph7_value *pTmp;     /* scratch for one element/assignment */
} scanf_sink;
/* Is this byte one of php's `isspace` set? Written out rather than asked of
 * libc, which answers by LC_CTYPE where php's scanner does not. */
static int ScanfIsSpace(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}
static int ScanfIsDigit(int c)
{
	return c >= '0' && c <= '9';
}
/*
 * Read a decimal run, saturating rather than wrapping -- strtoul's answer for
 * an over-wide run is its maximum, and php reads both the positional index and
 * the field width through it.
 */
static sxu64 ScanfReadUInt(const char *zIn,const char **pzEnd)
{
	sxu64 uVal = 0;
	int bOver = 0;
	while( ScanfIsDigit((unsigned char)zIn[0]) ){
		int d = zIn[0] - '0';
		if( uVal > (SXU64_HIGH - (sxu64)d) / 10 ){
			bOver = 1;
		}else{
			uVal = uVal * 10 + (sxu64)d;
		}
		zIn++;
	}
	if( pzEnd ){
		*pzEnd = zIn;
	}
	return bOver ? SXU64_HIGH : uVal;
}
/*
 * strtoll()/strtoull() over the accumulated buffer, which the scanners have
 * already restricted to a sign, an optional `0x` and digits of the base. The
 * conversion saturates the way the C library's does, which is what makes
 * `sscanf("9999999999999999999999","%d")` answer PHP_INT_MAX rather than a
 * wrapped number; `%u` then reinterprets that saturated width as unsigned.
 */
static sxu64 ScanfStrToNum(const char *zBuf,int iBase,int bUnsigned)
{
	const char *z = zBuf;
	sxu64 uVal = 0;
	int bNeg = 0, bOver = 0;
	if( z[0] == '+' || z[0] == '-' ){
		bNeg = (z[0] == '-');
		z++;
	}
	if( iBase == 16 && z[0] == '0' && (z[1] == 'x' || z[1] == 'X') ){
		z += 2;
	}
	for(;;){
		int d;
		int c = (unsigned char)z[0];
		if( ScanfIsDigit(c) ){
			d = c - '0';
		}else if( c >= 'a' && c <= 'f' ){
			d = c - 'a' + 10;
		}else if( c >= 'A' && c <= 'F' ){
			d = c - 'A' + 10;
		}else{
			break;
		}
		if( d >= iBase ){
			break;
		}
		if( uVal > (SXU64_HIGH - (sxu64)d) / (sxu64)iBase ){
			bOver = 1;
		}else{
			uVal = uVal * (sxu64)iBase + (sxu64)d;
		}
		z++;
	}
	if( bUnsigned ){
		/* strtoull saturates at its own maximum for EITHER sign, and negates
		 * modulo 2^64 otherwise -- which is why `%u` over "-5" answers
		 * 18446744073709551611. */
		if( bOver ){
			return SXU64_HIGH;
		}
		return bNeg ? (sxu64)(0 - uVal) : uVal;
	}
	if( bNeg ){
		if( bOver || uVal > (sxu64)SXI64_HIGH + 1 ){
			/* PHP_INT_MIN, spelled so no literal has to overflow to reach it. */
			return (sxu64)((sxu64)SXI64_HIGH + 1);
		}
		return (sxu64)(0 - uVal);
	}
	if( bOver || uVal > (sxu64)SXI64_HIGH ){
		return (sxu64)SXI64_HIGH;
	}
	return uVal;
}
/* Render a 64-bit value as unsigned decimal -- php's answer for a `%u` field
 * whose value will not fit a signed int, which it hands back as a STRING. */
static int ScanfUnsignedToStr(sxu64 uVal,char *zOut)
{
	char zTmp[24];
	int n = 0, i = 0;
	if( uVal == 0 ){
		zTmp[n++] = '0';
	}
	while( uVal > 0 ){
		zTmp[n++] = (char)('0' + (int)(uVal % 10));
		uVal /= 10;
	}
	while( n > 0 ){
		zOut[i++] = zTmp[--n];
	}
	zOut[i] = 0;
	return i;
}
/*
 * Build the character set a `%[` introduces. zSpec points just past the `[`
 * and a closing `]` is guaranteed to be there -- the validation pass refused
 * the format otherwise. Answers the position just past that `]`.
 *
 * The walk is php's, quirks included: a `]` or `-` written FIRST is a literal
 * member, a `-` written LAST is a literal member and so is the character
 * before it, and a range written backwards is silently turned around.
 */
static const char * ScanfBuildCharSet(scanf_charset *pSet,const char *zSpec,
	signed char *aChars,struct scanf_range *aRanges)
{
	const char *zCur;
	signed char start;
	pSet->exclude = 0;
	pSet->nchars = pSet->nranges = 0;
	pSet->chars = aChars;
	pSet->ranges = aRanges;
	if( zSpec[0] == '^' ){
		pSet->exclude = 1;
		zSpec++;
	}
	zCur = zSpec++;
	start = (signed char)zCur[0];
	if( zCur[0] == ']' || zCur[0] == '-' ){
		pSet->chars[pSet->nchars++] = (signed char)zCur[0];
		zCur = zSpec++;
	}
	while( zCur[0] != ']' ){
		if( zSpec[0] == '-' ){
			/* This may open a range, so hold it back rather than listing it. */
			start = (signed char)zCur[0];
		}else if( zCur[0] == '-' ){
			if( zSpec[0] == ']' ){
				/* A trailing dash is a member, and so is what came before it. */
				pSet->chars[pSet->nchars++] = start;
				pSet->chars[pSet->nchars++] = (signed char)zCur[0];
			}else{
				signed char stop;
				zCur = zSpec++;
				stop = (signed char)zCur[0];
				if( start < stop ){
					pSet->ranges[pSet->nranges].start = start;
					pSet->ranges[pSet->nranges].end = stop;
				}else{
					pSet->ranges[pSet->nranges].start = stop;
					pSet->ranges[pSet->nranges].end = start;
				}
				pSet->nranges++;
			}
		}else{
			pSet->chars[pSet->nchars++] = (signed char)zCur[0];
		}
		zCur = zSpec++;
	}
	return zSpec;
}
static int ScanfCharInSet(const scanf_charset *pSet,int c)
{
	signed char ch = (signed char)c;
	int i, match = 0;
	for( i = 0 ; i < pSet->nchars ; ++i ){
		if( pSet->chars[i] == ch ){
			match = 1;
			break;
		}
	}
	if( !match ){
		for( i = 0 ; i < pSet->nranges ; ++i ){
			if( pSet->ranges[i].start <= ch && ch <= pSet->ranges[i].end ){
				match = 1;
				break;
			}
		}
	}
	return pSet->exclude ? !match : match;
}
/*
 * php's "Bad scan conversion character" naming. A format that simply RAN OUT
 * lands here with the terminating NUL as the offending character, and php's
 * own formatter stops at it -- so that message really does end after the
 * opening quote, with no character and no closing one.
 */
static sxi32 ScanfBadConversion(ph7_context *pCtx,int c)
{
	if( c == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Bad scan conversion character \"");
	}
	return PH7_VmThrowException(pCtx,"ValueError",
		"Bad scan conversion character \"%c\"",c);
}
/*
 * Pass one: walk the format, refuse what php refuses, and answer how many
 * substitutions it makes.
 *
 * The assignment COUNTS are what the last two refusals are made of: a
 * positional index used twice is "assigned by multiple", and -- when the
 * caller passed variables rather than asking for an array -- an index no
 * specifier ever names is "not assigned by any", which is php's wording for
 * having passed too many variables.
 */
static sxi32 ScanfValidateFormat(ph7_context *pCtx,const char *zFmt,int nVar,int *pTotal)
{
	int aStatic[16];
	int *aAssign = aStatic;
	int nSpace = (int)SX_ARRAYSIZE(aStatic);
	int gotXpg = 0, gotSequential = 0;
	int objIndex = 0, xpgSize = 0;
	int i, nTotal;
	sxi32 rc = SXRET_OK;
	const char *zCur = zFmt;
	if( nVar > nSpace ){
		aAssign = (int *)ph7_context_alloc_chunk(pCtx,(unsigned int)nVar * sizeof(int),1,1);
		if( aAssign == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		nSpace = nVar;
	}
	for( i = 0 ; i < nSpace ; ++i ){
		aAssign[i] = 0;
	}
	while( zCur[0] != 0 ){
		const char *zCh = zCur++;
		int bSuppress = 0;
		if( zCh[0] != '%' ){
			continue;
		}
		zCh = zCur++;
		if( zCh[0] == '%' ){
			continue;
		}
		if( zCh[0] == '*' ){
			bSuppress = 1;
			zCh = zCur++;
			goto xpg_done;
		}
		if( ScanfIsDigit((unsigned char)zCh[0]) ){
			/* An XPG3 `%n$` specifier, if a `$` closes the run -- and a format
			 * may not carry both spellings. */
			const char *zEnd;
			sxu64 uVal = ScanfReadUInt(zCur - 1,&zEnd);
			int value;
			if( zEnd[0] != '$' ){
				goto not_xpg;
			}
			/* php reads this index through an `int`, so an over-wide run keeps
			 * only its low 32 bits -- reproduced rather than refused. */
			value = (int)(sxu32)uVal;
			zCur = zEnd + 1;
			zCh = zCur++;
			gotXpg = 1;
			if( gotSequential ){
				goto mixed_xpg;
			}
			if( value < 1 || (nVar && value > nVar) ){
				goto bad_index;
			}else if( nVar == 0 ){
				if( value > SCANF_MAX_ARGS ){
					goto bad_index;
				}
				xpgSize = (xpgSize > value) ? xpgSize : value;
			}
			objIndex = value - 1;
			goto xpg_done;
		}
not_xpg:
		gotSequential = 1;
		if( gotXpg ){
mixed_xpg:
			rc = PH7_VmThrowException(pCtx,"ValueError",
				"cannot mix \"%%\" and \"%%n$\" conversion specifiers");
			goto done;
		}
xpg_done:
		/* An optional field width. */
		if( ScanfIsDigit((unsigned char)zCh[0]) ){
			ScanfReadUInt(zCur - 1,&zCur);
			zCh = zCur++;
		}
		/* A size specifier is read and ignored, as php's is. */
		if( zCh[0] == 'l' || zCh[0] == 'L' || zCh[0] == 'h' ){
			zCh = zCur++;
		}
		if( !bSuppress && nVar && objIndex >= nVar ){
			goto bad_index;
		}
		switch( zCh[0] ){
			case 'n': case 'd': case 'D': case 'i': case 'o':
			case 'x': case 'X': case 'u': case 'f': case 'e':
			case 'E': case 'g': case 's': case 'c':
				break;
			case '[':
				/* The set has to CLOSE. `^` and a leading `]` are both members
				 * of the spelling rather than the set's end. */
				if( zCur[0] == 0 ){
					goto bad_set;
				}
				zCh = zCur++;
				if( zCh[0] == '^' ){
					if( zCur[0] == 0 ){
						goto bad_set;
					}
					zCh = zCur++;
				}
				if( zCh[0] == ']' ){
					if( zCur[0] == 0 ){
						goto bad_set;
					}
					zCh = zCur++;
				}
				while( zCh[0] != ']' ){
					if( zCur[0] == 0 ){
						goto bad_set;
					}
					zCh = zCur++;
				}
				break;
bad_set:
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"Unmatched [ in format string");
				goto done;
			default:
				rc = ScanfBadConversion(pCtx,(unsigned char)zCh[0]);
				goto done;
		}
		if( !bSuppress ){
			if( objIndex >= nSpace ){
				int nOld = nSpace;
				int *aNew;
				nSpace = xpgSize ? xpgSize : (nSpace + (int)SX_ARRAYSIZE(aStatic));
				/* Both arms grow past objIndex on every path that reaches here
				 * (xpgSize is at least objIndex+1 when it is set at all), but the
				 * copy below would run off a SHORTER buffer if one ever did not. */
				if( nSpace < nOld ){
					nSpace = nOld;
				}
				if( objIndex >= nSpace ){
					nSpace = objIndex + 1;
				}
				aNew = (int *)ph7_context_alloc_chunk(pCtx,
					(unsigned int)nSpace * sizeof(int),1,1);
				if( aNew == 0 ){
					rc = PH7_ContextMemoryError(pCtx);
					goto done;
				}
				for( i = 0 ; i < nOld ; ++i ){
					aNew[i] = aAssign[i];
				}
				if( aAssign != aStatic ){
					ph7_context_free_chunk(pCtx,aAssign);
				}
				aAssign = aNew;
			}
			aAssign[objIndex]++;
			objIndex++;
		}
	}
	nTotal = nVar;
	if( nTotal == 0 ){
		nTotal = xpgSize ? xpgSize : objIndex;
	}
	if( pTotal ){
		*pTotal = nTotal;
	}
	for( i = 0 ; i < nTotal && i < nSpace ; ++i ){
		if( aAssign[i] > 1 ){
			rc = PH7_VmThrowException(pCtx,"ValueError",
				"Variable is assigned by multiple \"%%n$\" conversion specifiers");
			goto done;
		}else if( !xpgSize && aAssign[i] == 0 ){
			rc = PH7_VmThrowException(pCtx,"ValueError",
				"Variable is not assigned by any conversion specifiers");
			goto done;
		}
	}
	if( aAssign != aStatic ){
		ph7_context_free_chunk(pCtx,aAssign);
	}
	return SXRET_OK;
bad_index:
	rc = gotXpg
		? PH7_VmThrowException(pCtx,"ValueError","\"%%n$\" argument index out of range")
		: PH7_VmThrowException(pCtx,"ValueError",
			"Different numbers of variable names and field specifiers");
done:
	if( aAssign != aStatic ){
		ph7_context_free_chunk(pCtx,aAssign);
	}
	return rc == SXRET_OK ? SXERR_ABORT : rc;
}
/* Hand one converted value to whichever sink this call has. */
static void ScanfAssign(scanf_sink *pSink,int objIndex,ph7_value *pVal)
{
	if( pSink->nVar > 0 ){
		if( objIndex >= 0 && objIndex < pSink->nVar ){
			PH7_VmStoreArgByRef(pSink->pCtx->pVm,pSink->apVar[objIndex],pVal);
		}
	}else if( pSink->pArray ){
		ph7_array_add_intkey_elem(pSink->pArray,objIndex,pVal);
	}
}
static void ScanfAssignInt(scanf_sink *pSink,int objIndex,sxi64 iVal)
{
	ph7_value_int64(pSink->pTmp,iVal);
	ScanfAssign(pSink,objIndex,pSink->pTmp);
}
static void ScanfAssignStr(scanf_sink *pSink,int objIndex,const char *zStr,int nLen)
{
	/* ph7_value_string() APPENDS, so the scratch has to be emptied first --
	 * the same slot carries every field this call converts. */
	ph7_value_string(pSink->pTmp,"",0);
	ph7_value_reset_string_cursor(pSink->pTmp);
	ph7_value_string(pSink->pTmp,zStr,nLen);
	ScanfAssign(pSink,objIndex,pSink->pTmp);
}
#ifndef PH7_OMIT_FLOATING_POINT
static void ScanfAssignDouble(scanf_sink *pSink,int objIndex,double rVal)
{
	ph7_value_double(pSink->pTmp,rVal);
	ScanfAssign(pSink,objIndex,pSink->pTmp);
}
#endif
/*
 * Pass two: scan zStr with zFmt and fill the sink.
 *
 * The whole family's return value is decided here: how many conversions were
 * PERFORMED, or the EOF answer when the input ran out before any of them did.
 */
static sxi32 ScanfExecFormat(ph7_context *pCtx,const char *zStr,const char *zFmt,
	scanf_sink *pSink,int *pnConv,int *pbEof)
{
	const char *zBase = zStr;
	const char *zCur = zFmt;
	int objIndex = 0;
	int nConv = 0, bUnderflow = 0;
	int op = 0, iBase = 0;
	*pbEof = 0;
	while( zCur[0] != 0 ){
		const char *zCh = zCur++;
		int flags = 0;
		int sch;
		sxu64 width;
		if( ScanfIsSpace((unsigned char)zCh[0]) ){
			/* Whitespace in the format eats whatever whitespace is there. */
			while( ScanfIsSpace((unsigned char)zStr[0]) ){
				if( zStr[0] == 0 ){
					goto done;
				}
				zStr++;
			}
			continue;
		}
		if( zCh[0] != '%' ){
literal:
			if( zStr[0] == 0 ){
				bUnderflow = 1;
				goto done;
			}
			sch = zStr[0];
			zStr++;
			if( zCh[0] != sch ){
				goto done;
			}
			continue;
		}
		zCh = zCur++;
		if( zCh[0] == '%' ){
			goto literal;
		}
		if( zCh[0] == '*' ){
			flags |= SCANF_SUPPRESS;
			zCh = zCur++;
		}else if( ScanfIsDigit((unsigned char)zCh[0]) ){
			const char *zEnd;
			sxu64 uVal = ScanfReadUInt(zCur - 1,&zEnd);
			if( zEnd[0] == '$' ){
				zCur = zEnd + 1;
				zCh = zCur++;
				objIndex = (int)(sxu32)uVal - 1;
			}
		}
		if( ScanfIsDigit((unsigned char)zCh[0]) ){
			width = ScanfReadUInt(zCur - 1,&zCur);
			zCh = zCur++;
		}else{
			width = 0;
		}
		if( zCh[0] == 'l' || zCh[0] == 'L' || zCh[0] == 'h' ){
			zCh = zCur++;
		}
		switch( zCh[0] ){
			case 'n':
				if( (flags & SCANF_SUPPRESS) == 0 ){
					ScanfAssignInt(pSink,objIndex++,(sxi64)(zStr - zBase));
				}
				nConv++;
				continue;
			case 'd': case 'D':
				op = 'i'; iBase = 10;
				break;
			case 'i':
				op = 'i'; iBase = 0;
				break;
			case 'o':
				op = 'i'; iBase = 8;
				break;
			case 'x': case 'X':
				op = 'i'; iBase = 16;
				break;
			case 'u':
				op = 'i'; iBase = 10;
				flags |= SCANF_UNSIGNED;
				break;
			case 'f': case 'e': case 'E': case 'g':
				op = 'f';
				break;
			case 's':
				op = 's';
				break;
			case 'c':
				/* php differs from Tcl here and allows a width; without one it
				 * reads a single character, and it never skips whitespace. */
				op = 's';
				flags |= SCANF_NOSKIP;
				if( width == 0 ){
					width = 1;
				}
				break;
			case '[':
				op = '[';
				flags |= SCANF_NOSKIP;
				break;
			default:
				/* Unreachable: the validation pass refused every other byte. */
				continue;
		}
		if( zStr[0] == 0 ){
			bUnderflow = 1;
			goto done;
		}
		if( (flags & SCANF_NOSKIP) == 0 ){
			while( zStr[0] != 0 && ScanfIsSpace((unsigned char)zStr[0]) ){
				zStr++;
			}
			if( zStr[0] == 0 ){
				bUnderflow = 1;
				goto done;
			}
		}
		switch( op ){
			case 's': {
				const char *zEnd;
				if( width == 0 ){
					width = SXU64_HIGH;
				}
				zEnd = zStr;
				while( zEnd[0] != 0 ){
					if( ScanfIsSpace((unsigned char)zEnd[0]) ){
						break;
					}
					zEnd++;
					if( --width == 0 ){
						break;
					}
				}
				if( (flags & SCANF_SUPPRESS) == 0 ){
					ScanfAssignStr(pSink,objIndex++,zStr,(int)(zEnd - zStr));
				}
				zStr = zEnd;
				break;
			}
			case '[': {
				scanf_charset sSet;
				const char *zEnd;
				signed char *aChars;
				struct scanf_range *aRanges;
				const char *zSpec = zCur;
				sxu32 nSpec;
				/* Measure the spec the way the validation pass read it: `^` and
				 * a `]` written FIRST belong to the spelling, so the closing
				 * bracket is the first one past them. The pass has already
				 * proved one is there. */
				if( zSpec[0] == '^' ){
					zSpec++;
				}
				if( zSpec[0] == ']' ){
					zSpec++;
				}
				while( zSpec[0] != ']' ){
					zSpec++;
				}
				nSpec = (sxu32)(zSpec - zCur) + 1;
				/* Two members can come out of one step (a trailing dash lists
				 * both itself and what preceded it), so leave room for it. */
				aChars = (signed char *)ph7_context_alloc_chunk(pCtx,
					(nSpec + 2) * (sxu32)sizeof(signed char),1,1);
				aRanges = (struct scanf_range *)ph7_context_alloc_chunk(pCtx,
					(nSpec + 1) * (sxu32)sizeof(struct scanf_range),1,1);
				if( aChars == 0 || aRanges == 0 ){
					return PH7_ContextMemoryError(pCtx);
				}
				if( width == 0 ){
					width = SXU64_HIGH;
				}
				zEnd = zStr;
				zCur = ScanfBuildCharSet(&sSet,zCur,aChars,aRanges);
				while( zEnd[0] != 0 ){
					if( !ScanfCharInSet(&sSet,(unsigned char)zEnd[0]) ){
						break;
					}
					zEnd++;
					if( --width == 0 ){
						break;
					}
				}
				ph7_context_free_chunk(pCtx,aChars);
				ph7_context_free_chunk(pCtx,aRanges);
				if( zStr == zEnd ){
					/* Nothing in the set is here: the scan stops. */
					goto done;
				}
				if( (flags & SCANF_SUPPRESS) == 0 ){
					ScanfAssignStr(pSink,objIndex++,zStr,(int)(zEnd - zStr));
				}
				zStr = zEnd;
				break;
			}
			case 'i': {
				char zBuf[SCANF_NUM_BUF];
				char *zOut = zBuf;
				sxu64 uVal;
				zBuf[0] = 0;
				if( width == 0 || width > SCANF_NUM_BUF - 1 ){
					width = SCANF_NUM_BUF - 1;
				}
				flags |= SCANF_SIGNOK | SCANF_NODIGITS | SCANF_NOZERO;
				for( ; width > 0 ; width-- ){
					int c = (unsigned char)zStr[0];
					switch( c ){
						case '0':
							if( iBase == 16 ){
								flags |= SCANF_XOK;
							}
							if( iBase == 0 ){
								iBase = 8;
								flags |= SCANF_XOK;
							}
							if( flags & SCANF_NOZERO ){
								flags &= ~(SCANF_SIGNOK | SCANF_NODIGITS | SCANF_NOZERO);
							}else{
								flags &= ~(SCANF_SIGNOK | SCANF_XOK | SCANF_NODIGITS);
							}
							goto add_int;
						case '1': case '2': case '3': case '4':
						case '5': case '6': case '7':
							if( iBase == 0 ){
								iBase = 10;
							}
							flags &= ~(SCANF_SIGNOK | SCANF_XOK | SCANF_NODIGITS);
							goto add_int;
						case '8': case '9':
							if( iBase == 0 ){
								iBase = 10;
							}
							if( iBase <= 8 ){
								break;
							}
							flags &= ~(SCANF_SIGNOK | SCANF_XOK | SCANF_NODIGITS);
							goto add_int;
						case 'A': case 'B': case 'C':
						case 'D': case 'E': case 'F':
						case 'a': case 'b': case 'c':
						case 'd': case 'e': case 'f':
							if( iBase <= 10 ){
								break;
							}
							flags &= ~(SCANF_SIGNOK | SCANF_XOK | SCANF_NODIGITS);
							goto add_int;
						case '+': case '-':
							if( flags & SCANF_SIGNOK ){
								flags &= ~SCANF_SIGNOK;
								goto add_int;
							}
							break;
						case 'x': case 'X':
							if( (flags & SCANF_XOK) && zOut == zBuf + 1 ){
								iBase = 16;
								flags &= ~SCANF_XOK;
								goto add_int;
							}
							break;
						default:
							break;
					}
					break;
add_int:
					*zOut++ = *zStr++;
					if( zStr[0] == 0 ){
						break;
					}
				}
				if( flags & SCANF_NODIGITS ){
					/* A sign and nothing else: this conversion never happened. */
					if( zStr[0] == 0 ){
						bUnderflow = 1;
					}
					goto done;
				}else if( zOut[-1] == 'x' || zOut[-1] == 'X' ){
					/* A `0x` whose digits never arrived: give the x back. */
					zOut--;
					zStr--;
				}
				*zOut = 0;
				if( (flags & SCANF_SUPPRESS) == 0 ){
					uVal = ScanfStrToNum(zBuf,iBase ? iBase : 10,
						(flags & SCANF_UNSIGNED) != 0);
					if( (flags & SCANF_UNSIGNED) && (sxi64)uVal < 0 ){
						char zNum[24];
						int nNum = ScanfUnsignedToStr(uVal,zNum);
						ScanfAssignStr(pSink,objIndex++,zNum,nNum);
					}else{
						ScanfAssignInt(pSink,objIndex++,(sxi64)uVal);
					}
				}
				break;
			}
			case 'f': {
				char zBuf[SCANF_NUM_BUF];
				char *zOut = zBuf;
				zBuf[0] = 0;
				if( width == 0 || width > SCANF_NUM_BUF - 1 ){
					width = SCANF_NUM_BUF - 1;
				}
				flags |= SCANF_SIGNOK | SCANF_NODIGITS | SCANF_PTOK | SCANF_EXPOK;
				for( ; width > 0 ; width-- ){
					int c = (unsigned char)zStr[0];
					switch( c ){
						case '0': case '1': case '2': case '3': case '4':
						case '5': case '6': case '7': case '8': case '9':
							flags &= ~(SCANF_SIGNOK | SCANF_NODIGITS);
							goto add_float;
						case '+': case '-':
							if( flags & SCANF_SIGNOK ){
								flags &= ~SCANF_SIGNOK;
								goto add_float;
							}
							break;
						case '.':
							if( flags & SCANF_PTOK ){
								flags &= ~(SCANF_SIGNOK | SCANF_PTOK);
								goto add_float;
							}
							break;
						case 'e': case 'E':
							/* An exponent needs a digit ahead of it. */
							if( (flags & (SCANF_NODIGITS | SCANF_EXPOK)) == SCANF_EXPOK ){
								flags = (flags & ~(SCANF_EXPOK | SCANF_PTOK))
									| SCANF_SIGNOK | SCANF_NODIGITS;
								goto add_float;
							}
							break;
						default:
							break;
					}
					break;
add_float:
					*zOut++ = *zStr++;
					if( zStr[0] == 0 ){
						break;
					}
				}
				if( flags & SCANF_NODIGITS ){
					if( flags & SCANF_EXPOK ){
						/* Not one digit anywhere: the scan stops here. */
						if( zStr[0] == 0 ){
							bUnderflow = 1;
						}
						goto done;
					}
					/* An exponent that never got its digits: give it back,
					 * and its sign with it. */
					zOut--;
					zStr--;
					if( zOut[0] != 'e' && zOut[0] != 'E' ){
						zOut--;
						zStr--;
					}
				}
				*zOut = 0;
				if( (flags & SCANF_SUPPRESS) == 0 ){
#ifndef PH7_OMIT_FLOATING_POINT
					ScanfAssignDouble(pSink,objIndex++,strtod(zBuf,0));
#else
					ScanfAssignInt(pSink,objIndex++,0);
#endif
				}
				break;
			}
			default:
				break;
		}
		nConv++;
	}
done:
	*pnConv = nConv;
	*pbEof = (bUnderflow && nConv == 0);
	return SXRET_OK;
}
/*
 * The body sscanf() and fscanf() share: validate, scan, and answer.
 *
 * With variables passed the answer is the number of conversions, or -1 when
 * the input ran out with none of them made. With none passed it is the array
 * of values, whose length the FORMAT decides -- a conversion that never
 * happened leaves its NULL in place -- or NULL for that same EOF case.
 */
PH7_PRIVATE sxi32 PH7_ScanfRun(ph7_context *pCtx,const char *zStr,int nStr,
	const char *zFmt,int nFmt,ph7_value **apVar,int nVar)
{
	scanf_sink sSink;
	char *zSubject = 0, *zFormat = 0;
	int nTotal = 0, nConv = 0, bEof = 0, i;
	sxi32 rc;
	if( nStr < 0 ){ nStr = 0; }
	if( nFmt < 0 ){ nFmt = 0; }
	/* Both strings are walked as C strings, php's own limitation. The copies
	 * carry two terminators because the walk may step one past the first. */
	zSubject = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nStr + 2,1,1);
	zFormat = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nFmt + 2,1,1);
	if( zSubject == 0 || zFormat == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nStr > 0 ){
		SyMemcpy(zStr,zSubject,(sxu32)nStr);
	}
	if( nFmt > 0 ){
		SyMemcpy(zFmt,zFormat,(sxu32)nFmt);
	}
	rc = ScanfValidateFormat(pCtx,zFormat,nVar,&nTotal);
	if( rc != SXRET_OK ){
		ph7_context_free_chunk(pCtx,zSubject);
		ph7_context_free_chunk(pCtx,zFormat);
		return rc;
	}
	sSink.pCtx = pCtx;
	sSink.apVar = apVar;
	sSink.nVar = nVar;
	sSink.pArray = 0;
	sSink.pTmp = ph7_context_new_scalar(pCtx);
	if( sSink.pTmp == 0 ){
		ph7_context_free_chunk(pCtx,zSubject);
		ph7_context_free_chunk(pCtx,zFormat);
		return PH7_ContextMemoryError(pCtx);
	}
	if( nVar == 0 ){
		sSink.pArray = ph7_context_new_array(pCtx);
		if( sSink.pArray == 0 ){
			ph7_context_free_chunk(pCtx,zSubject);
			ph7_context_free_chunk(pCtx,zFormat);
			return PH7_ContextMemoryError(pCtx);
		}
		/* php lays the whole answer out as NULLs first, so a conversion that
		 * never happens still has its place in the array. */
		ph7_value_null(sSink.pTmp);
		for( i = 0 ; i < nTotal ; ++i ){
			ph7_array_add_intkey_elem(sSink.pArray,i,sSink.pTmp);
		}
	}
	rc = ScanfExecFormat(pCtx,zSubject,zFormat,&sSink,&nConv,&bEof);
	ph7_context_free_chunk(pCtx,zSubject);
	ph7_context_free_chunk(pCtx,zFormat);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( bEof ){
		if( nVar > 0 ){
			ph7_result_int(pCtx,-1);
		}else{
			ph7_result_null(pCtx);
		}
	}else if( nVar > 0 ){
		ph7_result_int(pCtx,nConv);
	}else{
		ph7_result_value(pCtx,sSink.pArray);
	}
	return SXRET_OK;
}
#endif /* !PH7_DISABLE_BUILTIN_FUNC || !PH7_DISABLE_DISK_IO */
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * array|int|null sscanf(string $string, string $format, mixed &...$vars)
 *  Parse $string according to $format.
 */
PH7_PRIVATE int PH7_builtin_sscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStr,*zFmt;
	int nStr = 0, nFmt = 0;
	if( nArg < 2 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zStr = ph7_value_to_string(apArg[0],&nStr);
	zFmt = ph7_value_to_string(apArg[1],&nFmt);
	return (int)PH7_ScanfRun(pCtx,zStr,nStr,zFmt,nFmt,&apArg[2],nArg - 2);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
