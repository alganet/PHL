/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <string.h>  /* memset (field padding) */
/*
 * Section:
 *    Binary strings: pack() and unpack().
 * Status:
 *    Stable.
 *
 * The format string is a little language php inherited from Perl, and both
 * builtins read the same alphabet of 24 codes. A code is one letter, optionally
 * followed by a REPEATER -- a decimal count, or `*` for "as many as there are".
 * What the repeater counts is the code's own business: `N4` is four 32-bit
 * words, `a4` is ONE four-byte string field, `x4` is four NUL bytes, and `@4` is
 * an absolute position rather than a count at all.
 *
 * Modelled on php 8.5's ext/standard/pack.c, whose behaviour is the contract
 * here -- including the parts a re-derivation gets wrong: which codes consume an
 * argument, that a field wider than its value is PADDED rather than refused, and
 * that a diagnostic is raised where php raises it (the preprocessing pass finds
 * an unknown code before a single byte is produced, while a short hex string is
 * only noticed while the bytes are being laid down).
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifdef PH7_NEED_BUILTIN_REG
/*
 * php sizes every count in this family with a C `int` and refuses an output
 * position that would overflow one, so the whole engine is bounded by INT_MAX
 * and so is this.
 */
#define PACK_INT_MAX 2147483647
/*
 * Byte order of a fixed-width field. MACHINE is what `s`, `S`, `i`, `I`, `l`,
 * `L`, `q`, `Q`, `f` and `d` mean -- the host's own order, which is what makes
 * them the codes to avoid in a file format meant to travel.
 */
#define PACK_MACHINE 0
#define PACK_LITTLE  1
#define PACK_BIG     2
/*
 * Is the host little-endian? Read off the storage of a known value rather than
 * taken from a build-time macro, so a port whose makefile gets its endianness
 * wrong still packs correctly.
 */
static int PackHostIsLittle(void)
{
	static const sxu32 uProbe = 1;
	return ((const unsigned char *)&uProbe)[0] == 1;
}
/*
 * Lay the low nSize bytes of uVal into zOut in the requested order. Written
 * with shifts rather than as a memcpy of a native type, so the answer is the
 * same on either host: a BIG field is most-significant byte first whatever the
 * host does, a LITTLE field least-significant first, and a MACHINE field is
 * whichever of the two the host uses -- which is what php's byte-map tables say.
 */
static void PackPutInt(char *zOut,sxu64 uVal,int nSize,int iOrder)
{
	int bLittle = (iOrder == PACK_LITTLE) ||
		(iOrder == PACK_MACHINE && PackHostIsLittle());
	int i;
	for( i = 0 ; i < nSize ; ++i ){
		int nShift = bLittle ? i : (nSize - 1 - i);
		zOut[i] = (char)((uVal >> (8 * nShift)) & 0xFF);
	}
}
/*
 * Read nSize bytes back out of zIn as an unsigned value in the given order --
 * PackPutInt's inverse, and the only reader unpack() uses. Nothing wider than a
 * byte is ever dereferenced, so an unaligned field (which is the ordinary case:
 * `unpack('Ca/Nb', ...)` puts the 32-bit one at offset 1) is not a misaligned
 * load.
 */
static sxu64 PackGetInt(const char *zIn,int nSize,int iOrder)
{
	int bLittle = (iOrder == PACK_LITTLE) ||
		(iOrder == PACK_MACHINE && PackHostIsLittle());
	sxu64 uVal = 0;
	int i;
	for( i = 0 ; i < nSize ; ++i ){
		int nShift = bLittle ? i : (nSize - 1 - i);
		uVal |= ((sxu64)(unsigned char)zIn[i]) << (8 * nShift);
	}
	return uVal;
}
/*
 * A float and a double travel as their IEEE-754 bit pattern, so all four are the
 * integer routines above applied to the value's own storage. `f`/`d` are the
 * host's order; `g`/`e` little-endian, `G`/`E` big.
 */
static void PackPutFloat(char *zOut,float fVal,int iOrder)
{
	sxu32 uBits = 0;
	SyMemcpy((const void *)&fVal,(void *)&uBits,(sxu32)sizeof(uBits));
	PackPutInt(zOut,(sxu64)uBits,(int)sizeof(uBits),iOrder);
}
static void PackPutDouble(char *zOut,double dVal,int iOrder)
{
	sxu64 uBits = 0;
	SyMemcpy((const void *)&dVal,(void *)&uBits,(sxu32)sizeof(uBits));
	PackPutInt(zOut,uBits,(int)sizeof(uBits),iOrder);
}
static float PackGetFloat(const char *zIn,int iOrder)
{
	sxu32 uBits = (sxu32)PackGetInt(zIn,(int)sizeof(uBits),iOrder);
	float fVal = 0.0f;
	SyMemcpy((const void *)&uBits,(void *)&fVal,(sxu32)sizeof(fVal));
	return fVal;
}
static double PackGetDouble(const char *zIn,int iOrder)
{
	sxu64 uBits = PackGetInt(zIn,(int)sizeof(uBits),iOrder);
	double dVal = 0.0;
	SyMemcpy((const void *)&uBits,(void *)&dVal,(sxu32)sizeof(dVal));
	return dVal;
}
/*
 * Width in bytes of one repetition of a fixed-width code, and the order it is
 * written in. Answers 0 for a code with no fixed width of its own
 * (`a`/`A`/`Z`/`h`/`H`/`x`/`X`/`@`), which every caller screens for first.
 *
 * `i`/`I` are php's one platform-sized code (`sizeof(int)`), 4 on every target
 * PHL builds for but asked rather than assumed.
 */
static int PackFixedWidth(int code,int *piOrder)
{
	int nSize = 0, iOrder = PACK_MACHINE;
	switch( code ){
		case 'c': case 'C': nSize = 1; break;
		case 's': case 'S': nSize = 2; break;
		case 'n': nSize = 2; iOrder = PACK_BIG;    break;
		case 'v': nSize = 2; iOrder = PACK_LITTLE; break;
		case 'i': case 'I': nSize = (int)sizeof(int); break;
		case 'l': case 'L': nSize = 4; break;
		case 'N': nSize = 4; iOrder = PACK_BIG;    break;
		case 'V': nSize = 4; iOrder = PACK_LITTLE; break;
		case 'q': case 'Q': nSize = 8; break;
		case 'J': nSize = 8; iOrder = PACK_BIG;    break;
		case 'P': nSize = 8; iOrder = PACK_LITTLE; break;
		case 'f': nSize = (int)sizeof(float); break;
		case 'g': nSize = (int)sizeof(float);  iOrder = PACK_LITTLE; break;
		case 'G': nSize = (int)sizeof(float);  iOrder = PACK_BIG;    break;
		case 'd': nSize = (int)sizeof(double); break;
		case 'e': nSize = (int)sizeof(double); iOrder = PACK_LITTLE; break;
		case 'E': nSize = (int)sizeof(double); iOrder = PACK_BIG;    break;
		default: break;
	}
	if( piOrder ){
		*piOrder = iOrder;
	}
	return nSize;
}
/* Does this code take its value as a STRING rather than as a number? */
static int PackCodeTakesString(int code)
{
	return code == 'a' || code == 'A' || code == 'Z' ||
		code == 'h' || code == 'H';
}
/* Is this code one of the three that produce bytes out of no argument at all? */
static int PackCodeTakesNoArg(int code)
{
	return code == 'x' || code == 'X' || code == '@';
}
/*
 * Read the repeater that follows a format code: a run of decimal digits, `*`
 * (answered as -1), or nothing at all (answered as 1). *pzCur is advanced past
 * whatever was consumed.
 *
 * php parses the digits with atoi(), so past INT_MAX the answer is the C
 * library's: glibc truncates the parsed long into an int, and
 * `pack('a99999999999','xy')` really does allocate the 1215752191 bytes that
 * wrap lands on while `pack('x2147483648')` wraps NEGATIVE and is read as `*`.
 * PHL refuses the whole class instead, using the message php raises for the same
 * overflow one step later. A count no int can hold is not a count.
 */
static sxi32 PackReadRepeat(const char **pzCur,const char *zEnd,sxi64 *piRepeat)
{
	const char *zCur = *pzCur;
	sxi64 iVal = 0;
	*piRepeat = 1;
	if( zCur >= zEnd ){
		return SXRET_OK;
	}
	if( zCur[0] == '*' ){
		*pzCur = zCur + 1;
		*piRepeat = -1;
		return SXRET_OK;
	}
	if( zCur[0] < '0' || zCur[0] > '9' ){
		return SXRET_OK;
	}
	while( zCur < zEnd && zCur[0] >= '0' && zCur[0] <= '9' ){
		if( iVal <= PACK_INT_MAX ){
			iVal = iVal * 10 + (zCur[0] - '0');
		}
		zCur++;
	}
	*pzCur = zCur;
	*piRepeat = iVal;
	return iVal > PACK_INT_MAX ? SXERR_OVERFLOW : SXRET_OK;
}
/*
 * One preprocessed format entry: the code and its repeater, with `*` already
 * resolved into the count it stands for. php builds the same two arrays for the
 * same reason -- the whole format has to be validated, and the output size
 * known, before a single byte is written.
 */
struct PackEntry {
	int code;
	sxi64 iRepeat;
};
/*
 * The string one `a`/`A`/`Z`/`h`/`H` entry consumes, converted ONCE. php
 * converts the argument in place in its own argument stack, so an array
 * argument warns `Array to string conversion` once however many passes read it;
 * PHL must not write to the caller's value (a spread `pack('a*', ...$rows)`
 * hands the array's own element slots straight in), so the conversion is kept
 * here instead.
 */
struct PackStrArg {
	ph7_value sVal;     /* The scratch copy that owns the bytes below */
	const char *zStr;
	int nStr;
	int bDone;          /* Converted already (the format may read it twice) */
	int bNotStringable; /* php's catchable Error, predicted rather than raised */
};
/*
 * Convert value argument iIdx to a string once and answer it, emitting php's
 * user-visible coercion diagnostics the first time.
 *
 * Three outcomes: SXRET_OK, SXERR_ABORT for a value that cannot become a string
 * at all (an object with no __toString(), php's catchable Error -- PREDICTED
 * here rather than raised, so the caller decides when it may interrupt the
 * builtin), and any other error status for a __toString() that itself threw,
 * which cannot be predicted and stops the call where it happened.
 */
static sxi32 PackStringArg(
	ph7_context *pCtx,
	ph7_value **apArg,
	struct PackStrArg *aStr,
	int iIdx,
	const char **pzStr,
	int *pnStr
){
	struct PackStrArg *pSlot = &aStr[iIdx];
	sxi32 rc = SXRET_OK;
	if( !pSlot->bDone ){
		pSlot->bDone = 1;
		pSlot->zStr = "";
		pSlot->nStr = 0;
		PH7_MemObjInit(pCtx->pVm,&pSlot->sVal);
		PH7_MemObjLoad(apArg[iIdx + 1],&pSlot->sVal);
		if( PH7_MemObjIsNotStringable(&pSlot->sVal) ){
			pSlot->bNotStringable = 1;
		}else{
			rc = PH7_ValueToStringUV(pCtx,&pSlot->sVal,&pSlot->zStr,&pSlot->nStr);
			if( rc != SXRET_OK ){
				pSlot->zStr = "";
				pSlot->nStr = 0;
			}
		}
	}
	*pzStr = pSlot->zStr;
	*pnStr = pSlot->nStr;
	return pSlot->bNotStringable ? SXERR_ABORT : rc;
}
/*
 * string pack(string $format, mixed ...$values)
 *  Pack the given values into a binary string according to $format.
 *
 * Three passes, php's own: preprocess the format (which resolves every count,
 * consumes every argument and raises every ValueError), measure the output, then
 * write it. Splitting them is what lets `X` and `@` move the cursor backwards
 * over bytes that have already been produced.
 */
PH7_PRIVATE int PH7_builtin_pack(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFmt,*zCur,*zEnd;
	struct PackEntry *aEntry = 0;
	struct PackStrArg *aStr = 0;
	int nEntry = 0, nFmt = 0, nValue = 0, iValue = 0, i;
	sxi64 iPos = 0, iSize = 0;
	char *zOut = 0;
	sxi32 rc = PH7_OK;
	int iThrowArg = 0;  /* 1-based apArg index of the first unconvertible value */
	if( nArg < 1 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	nValue = nArg - 1;
	if( (sxu32)nFmt > (sxu32)(PACK_INT_MAX / (int)sizeof(struct PackEntry)) ){
		/* One entry per format byte, so a format long enough to overflow the
		 * entry array's own size is refused rather than under-allocated. */
		return PH7_ContextMemoryError(pCtx);
	}
	if( nFmt > 0 ){
		aEntry = (struct PackEntry *)ph7_context_alloc_chunk(pCtx,
			(unsigned int)((sxu32)nFmt * sizeof(struct PackEntry)),TRUE,TRUE);
		if( aEntry == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
	}
	if( nValue > 0 ){
		aStr = (struct PackStrArg *)ph7_context_alloc_chunk(pCtx,
			(unsigned int)((sxu32)nValue * sizeof(struct PackStrArg)),TRUE,TRUE);
		if( aStr == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
	}
	/* Pass 1: read the format, resolve every repeater, consume every argument. */
	zCur = zFmt;
	zEnd = &zFmt[nFmt];
	while( zCur < zEnd ){
		int code = (unsigned char)zCur[0];
		sxi64 iRepeat;
		int bOverflow;
		zCur++;
		bOverflow = PackReadRepeat(&zCur,zEnd,&iRepeat) != SXRET_OK;
		if( !PackCodeTakesNoArg(code) && !PackCodeTakesString(code)
		 && PackFixedWidth(code,0) < 1 ){
			/* Whether the code EXISTS is decided before anything its repeater
			 * says, which is php's order: a format is validated left to right and
			 * an unknown letter is the first thing wrong with it. */
			rc = PH7_VmThrowException(pCtx,"ValueError",
				"Type %c: unknown format code",code);
			goto Done;
		}
		if( bOverflow ){
			rc = PH7_VmThrowException(pCtx,"ValueError",
				"Type %c: integer overflow in format string",code);
			goto Done;
		}
		if( PackCodeTakesNoArg(code) ){
			if( iRepeat < 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Type %c: '*' ignored",code);
				iRepeat = 1;
			}
		}else if( PackCodeTakesString(code) ){
			if( iValue >= nValue ){
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"Type %c: not enough arguments",code);
				goto Done;
			}
			if( iRepeat < 0 ){
				/* `*` is the argument's own length, so the argument has to become a
				 * string HERE -- and one that cannot stops the call where php's
				 * try_convert_to_string() stops it, ahead of the unused-argument
				 * warning below rather than after it. */
				const char *zStr; int nStr;
				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);
				if( rcStr == SXERR_ABORT ){
					rc = PH7_MemObjToStringUV(apArg[iValue + 1]);
					goto Done;
				}else if( rcStr != SXRET_OK ){
					rc = rcStr;
					goto Done;
				}
				/* Z is always NUL-terminated, so `Z*` is one byte longer than the
				 * string it was given: pack('Z*','aa') is "aa\0". */
				iRepeat = (sxi64)nStr + (code == 'Z' ? 1 : 0);
			}
			iValue++;
		}else if( PackFixedWidth(code,0) > 0 ){
			sxi64 iNext;
			if( iRepeat < 0 ){
				iRepeat = nValue - iValue;
			}
			iNext = (sxi64)iValue + iRepeat;
			if( iNext > nValue ){
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"Type %c: too few arguments",code);
				goto Done;
			}
			iValue = (int)iNext;
		}
		aEntry[nEntry].code = code;
		aEntry[nEntry].iRepeat = iRepeat;
		nEntry++;
	}
	if( iValue < nValue ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"%d arguments unused",nValue - iValue);
	}
	/* Pass 2: measure. The answer is the HIGHEST position the cursor reaches,
	 * which is not the last one: `@8X4` ends at 4 having produced 8. */
	for( i = 0 ; i < nEntry ; ++i ){
		int code = aEntry[i].code;
		sxi64 iRepeat = aEntry[i].iRepeat;
		if( code == 'X' ){
			iPos -= iRepeat;
			if( iPos < 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Type %c: outside of string",code);
				iPos = 0;
			}
		}else if( code == '@' ){
			iPos = iRepeat;
		}else{
			sxi64 nUnit = 1, nCount = iRepeat;
			if( code == 'h' || code == 'H' ){
				/* Two hex digits to the byte, an odd count rounding up. */
				nCount = iRepeat / 2 + (iRepeat % 2);
			}else if( !PackCodeTakesString(code) && code != 'x' ){
				nUnit = PackFixedWidth(code,0);
			}
			if( (PACK_INT_MAX - iPos) / nUnit < nCount ){
				rc = PH7_VmThrowException(pCtx,"ValueError",
					"Type %c: integer overflow in format string",code);
				goto Done;
			}
			iPos += nCount * nUnit;
		}
		if( iSize < iPos ){
			iSize = iPos;
		}
	}
	if( iSize > 0 ){
		zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)iSize,TRUE,TRUE);
		if( zOut == 0 ){
			rc = PH7_ContextMemoryError(pCtx);
			goto Done;
		}
	}
	/* Pass 3: write. */
	iPos = 0;
	iValue = 0;
	for( i = 0 ; i < nEntry ; ++i ){
		int code = aEntry[i].code;
		sxi64 iRepeat = aEntry[i].iRepeat;
		int iOrder = PACK_MACHINE, nWidth;
		sxi64 n;
		switch( code ){
			case 'a': case 'A': case 'Z': {
				/* One field of exactly iRepeat bytes: the value truncated to fit,
				 * or padded out to the width. `A` pads with spaces, the other two
				 * with NULs, and `Z` keeps its last byte for the terminator. */
				sxi64 iRoom = (code == 'Z') ? (iRepeat > 0 ? iRepeat - 1 : 0) : iRepeat;
				const char *zStr; int nStr;
				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);
				if( rcStr == SXERR_ABORT ){
					/* php raises the Error here and lets pack() run to the end, so
					 * PHL cannot raise it here -- an enclosing catch would run
					 * before the rest of the format. Remember it for the exit. */
					if( iThrowArg == 0 ){
						iThrowArg = iValue + 1;
					}
				}else if( rcStr != SXRET_OK ){
					rc = rcStr;
					goto Done;
				}
				if( iRoom > nStr ){
					iRoom = nStr;
				}
				if( iRepeat > 0 ){
					memset(&zOut[iPos],(code == 'A') ? ' ' : '\0',(size_t)iRepeat);
				}
				if( iRoom > 0 ){
					SyMemcpy((const void *)zStr,(void *)&zOut[iPos],(sxu32)iRoom);
				}
				iPos += iRepeat;
				iValue++;
				break;
			}
			case 'h': case 'H': {
				/* One nibble per input character: `h` fills the low nibble of a
				 * byte first, `H` the high one. */
				int nShift = (code == 'h') ? 0 : 4;
				int bFirst = 1;
				const char *zStr; int nStr;
				sxi32 rcStr = PackStringArg(pCtx,apArg,aStr,iValue,&zStr,&nStr);
				if( rcStr == SXERR_ABORT ){
					if( iThrowArg == 0 ){
						iThrowArg = iValue + 1;
					}
				}else if( rcStr != SXRET_OK ){
					rc = rcStr;
					goto Done;
				}
				if( iRepeat > nStr ){
					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
						"Type %c: not enough characters in string",code);
					iRepeat = nStr;
				}
				iPos--;
				for( n = 0 ; n < iRepeat ; ++n ){
					int c = (unsigned char)zStr[n];
					int digit;
					if( c >= '0' && c <= '9' ){
						digit = c - '0';
					}else if( c >= 'A' && c <= 'F' ){
						digit = c - ('A' - 10);
					}else if( c >= 'a' && c <= 'f' ){
						digit = c - ('a' - 10);
					}else{
						ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
							"Type %c: illegal hex digit %c",code,c);
						digit = 0;
					}
					if( bFirst ){
						/* A byte starts here, so the nibble is written into a
						 * cleared byte rather than merged into an old one. */
						zOut[++iPos] = 0;
						bFirst = 0;
					}else{
						bFirst = 1;
					}
					zOut[iPos] = (char)((unsigned char)zOut[iPos] | (digit << nShift));
					nShift = (nShift + 4) & 7;
				}
				iPos++;
				iValue++;
				break;
			}
			case 'x':
				if( iRepeat > 0 ){
					memset(&zOut[iPos],'\0',(size_t)iRepeat);
				}
				iPos += iRepeat;
				break;
			case 'X':
				iPos -= iRepeat;
				if( iPos < 0 ){
					/* Already reported by the measuring pass. */
					iPos = 0;
				}
				break;
			case '@':
				if( iRepeat > iPos ){
					memset(&zOut[iPos],'\0',(size_t)(iRepeat - iPos));
				}
				iPos = iRepeat;
				break;
			default:
				/* The numeric codes read their argument through
				 * ph7_value_to_int64 / ph7_value_to_double, which coerce IN PLACE
				 * -- which is safe only because each value is consumed by exactly
				 * one entry and a builtin's arguments are copies (a spread of an
				 * array, and a reference inside one, both leave the caller's
				 * elements alone; verified against the oracle). The string codes
				 * cannot take that route: they are read twice, once to measure and
				 * once to write, and a second conversion would emit a second
				 * `Array to string conversion`. */
				nWidth = PackFixedWidth(code,&iOrder);
				for( n = 0 ; n < iRepeat ; ++n ){
					ph7_value *pVal = apArg[iValue + 1];
					if( code == 'f' || code == 'g' || code == 'G' ){
						PackPutFloat(&zOut[iPos],(float)ph7_value_to_double(pVal),iOrder);
					}else if( code == 'd' || code == 'e' || code == 'E' ){
						PackPutDouble(&zOut[iPos],ph7_value_to_double(pVal),iOrder);
					}else{
						PackPutInt(&zOut[iPos],(sxu64)ph7_value_to_int64(pVal),nWidth,iOrder);
					}
					iPos += nWidth;
					iValue++;
				}
				break;
		}
	}
	ph7_result_string(pCtx,iPos > 0 ? zOut : "",(int)iPos);
	if( iThrowArg ){
		/* The format ran to completion and the result is in place; raise php's
		 * Error now. The unwind discards that result, exactly as php's does. */
		rc = PH7_MemObjToStringUV(apArg[iThrowArg]);
	}
Done:
	for( i = 0 ; i < nValue ; ++i ){
		if( aStr[i].bDone ){
			PH7_MemObjRelease(&aStr[i].sVal);
		}
	}
	return rc;
}
/*
 * php truncates an unpack() element NAME at 200 bytes. The name is whatever
 * follows the repeater up to the next '/', so it is the format's own text and
 * nothing bounds it otherwise.
 */
#define UNPACK_MAX_NAME 200
/*
 * Store one unpacked value under the name this entry gives it. php's three
 * naming rules, in its own order: an entry with NO name is keyed by its
 * 1-based POSITION within that entry (`unpack('C*', $s)` answers 1,2,3...), a
 * single repetition takes the name as written, and a repeated one has the
 * 1-based index appended (`unpack('C3n', $s)` answers n1, n2, n3).
 *
 * The key goes in as a php key, so a name that spells an integer becomes an
 * integer key exactly as `zend_symtable_update` makes it one.
 */
static sxi32 UnpackStore(
	ph7_context *pCtx,
	ph7_value *pArray,
	ph7_value *pKey,
	ph7_value *pVal,
	const char *zName,
	int nName,
	sxi64 iIndex,
	int bIndexed
){
	if( nName < 1 ){
		ph7_value_int64(pKey,iIndex);
	}else if( !bIndexed ){
		ph7_value_string(pKey,zName,nName);
	}else{
		char zNum[32];
		int nNum;
		ph7_value_string(pKey,zName,nName);
		nNum = SyBufferFormat(zNum,sizeof(zNum),"%qd",iIndex);
		ph7_value_string(pKey,zNum,nNum);
	}
	if( ph7_array_add_elem(pArray,pKey,pVal) != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_reset_string_cursor(pKey);
	return PH7_OK;
}
/*
 * array|false unpack(string $format, string $string, int $offset = 0)
 *  Read a binary string back into an array according to $format.
 *
 * The mirror of pack(), with two differences that are php's and not
 * symmetries: the format is a '/'-separated list of NAMED entries rather than a
 * bare run of codes, and a repeater means something slightly different for the
 * string codes -- `a5` is one FIVE-BYTE field here as it is there, but `C3` is
 * three separate elements rather than three bytes of one.
 *
 * Answers FALSE (after a warning) for input that runs out mid-field, which is
 * why the whole array is discarded rather than returned half-filled.
 */
PH7_PRIVATE int PH7_builtin_unpack(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFmt,*zCur,*zFmtEnd,*zIn;
	ph7_value *pArray,*pVal,*pKey;
	int nFmt = 0, nIn = 0;
	sxi64 iOffset = 0, iPos = 0;
	sxi32 rc = PH7_OK;
	if( nArg < 2 ){
		/* Arity is enforced from aBuiltinSig[] before the call. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFmt = ph7_value_to_string(apArg[0],&nFmt);
	zIn = ph7_value_to_string(apArg[1],&nIn);
	if( nArg > 2 ){
		iOffset = ph7_value_to_int64(apArg[2]);
	}
	if( iOffset < 0 || iOffset > nIn ){
		/* php names argument #2 as `$data` here and `$string` in the signature;
		 * the message is its own, verbatim. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"unpack(): Argument #3 ($offset) must be contained in argument #2 ($data)");
	}
	zIn += iOffset;
	nIn -= (int)iOffset;
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	pKey = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 || pKey == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	zCur = zFmt;
	zFmtEnd = &zFmt[nFmt];
	while( zCur < zFmtEnd ){
		int code = (unsigned char)zCur[0];
		const char *zName;
		int nName, iOrder = PACK_MACHINE;
		sxi64 iRepeat, iDeclared, iSize = 0, i;
		zCur++;
		if( PackReadRepeat(&zCur,zFmtEnd,&iRepeat) != SXRET_OK ){
			/* Unlike pack()'s atoi, php range-checks this one and answers with a
			 * warning and FALSE rather than a wrapped count. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Type %c: integer overflow",code);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* The NAME is everything up to the next '/' -- including spaces, digits
		 * and anything else, since only the separator ends it. */
		zName = zCur;
		while( zCur < zFmtEnd && zCur[0] != '/' ){
			zCur++;
		}
		nName = (int)(zCur - zName);
		if( nName > UNPACK_MAX_NAME ){
			nName = UNPACK_MAX_NAME;
		}
		iDeclared = iRepeat;
		switch( code ){
			case 'X':
				/* Consumes nothing and moves the cursor BACK one byte per
				 * repetition; the negative size is what does the moving. */
				iSize = -1;
				if( iRepeat < 0 ){
					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
						"Type %c: '*' ignored",code);
					iRepeat = 1;
				}
				break;
			case '@':
				iSize = 0;
				break;
			case 'a': case 'A': case 'Z':
				/* One field of that many bytes, not that many fields. */
				iSize = iRepeat;
				iRepeat = 1;
				break;
			case 'h': case 'H':
				/* The repeater counts NIBBLES, so the field is half as many
				 * bytes, rounded up. */
				iSize = iRepeat > 0 ? (iRepeat + 1) / 2 : iRepeat;
				iRepeat = 1;
				break;
			case 'x':
				iSize = 1;
				break;
			default:
				iSize = PackFixedWidth(code,&iOrder);
				if( iSize < 1 ){
					rc = PH7_VmThrowException(pCtx,"ValueError",
						"Invalid format type %c",code);
					return rc;
				}
				break;
		}
		for( i = 0 ; i != iRepeat ; ++i ){
			int bIndexed = (iRepeat != 1);
			/* The value slot is reused across repetitions and a string goes in by
			 * APPEND, so the previous element has to be let go of first. */
			ph7_value_reset_string_cursor(pVal);
			if( iSize > 0 && (sxi64)PACK_INT_MAX - iSize + 1 < iPos ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Type %c: integer overflow",code);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			if( iPos + iSize > nIn ){
				if( iRepeat < 0 ){
					/* A `*` run simply stops when the input does. */
					break;
				}
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Type %c: not enough input values, need %d values but only "
					"%qd %s provided",
					code,(int)iSize,(sxi64)(nIn - iPos),
					(nIn - iPos) == 1 ? "was" : "were");
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			switch( code ){
				case 'a': case 'A': case 'Z': {
					/* All three read the same run of bytes and differ only in
					 * what they keep: `a` everything, `A` minus its trailing
					 * whitespace and NULs, `Z` up to the first NUL. */
					sxi64 iLen = nIn - iPos;
					if( iSize >= 0 && iLen > iSize ){
						iLen = iSize;
					}
					iSize = iLen;
					if( code == 'A' ){
						while( --iLen >= 0 ){
							int c = (unsigned char)zIn[iPos + iLen];
							if( c != '\0' && c != ' ' && c != '\t'
							 && c != '\r' && c != '\n' ){
								break;
							}
						}
						iLen++;
					}else if( code == 'Z' ){
						sxi64 s;
						for( s = 0 ; s < iLen ; ++s ){
							if( zIn[iPos + s] == '\0' ){
								break;
							}
						}
						iLen = s;
					}
					ph7_value_string(pVal,&zIn[iPos],(int)iLen);
					break;
				}
				case 'h': case 'H': {
					/* One hex digit per nibble, in the order that code fills
					 * them. An ODD declared count drops the last digit. */
					sxi64 iLen = ((sxi64)nIn - iPos) * 2;
					int nShift = (code == 'h') ? 0 : 4;
					int bFirst = 1;
					sxi64 iIn = 0, iOut;
					char *zHex;
					if( iSize > PACK_INT_MAX / 2 ){
						/* Asked HERE, where php asks it: a repeater this large has
						 * already failed the input-length check above unless the
						 * input is enormous. */
						return PH7_VmThrowException(pCtx,"ValueError",
							"unpack(): Argument #1 ($format) repeater must be less than "
							"or equal to %d",PACK_INT_MAX / 2);
					}
					if( iSize >= 0 && iLen > iSize * 2 ){
						iLen = iSize * 2;
					}
					if( iLen > 0 && iDeclared > 0 ){
						iLen -= iDeclared % 2;
					}
					zHex = (char *)ph7_context_alloc_chunk(pCtx,
						(unsigned int)(iLen > 0 ? iLen : 1),FALSE,TRUE);
					if( zHex == 0 ){
						return PH7_ContextMemoryError(pCtx);
					}
					for( iOut = 0 ; iOut < iLen ; ++iOut ){
						int c = (((unsigned char)zIn[iPos + iIn]) >> nShift) & 0x0F;
						zHex[iOut] = (char)(c < 10 ? c + '0' : c + ('a' - 10));
						nShift = (nShift + 4) & 7;
						if( bFirst ){
							bFirst = 0;
						}else{
							iIn++;
							bFirst = 1;
						}
					}
					ph7_value_string(pVal,zHex,(int)(iLen > 0 ? iLen : 0));
					break;
				}
				case 'c': case 'C': {
					int c = (unsigned char)zIn[iPos];
					ph7_value_int64(pVal,code == 'c' ? (sxi64)(signed char)c : (sxi64)c);
					break;
				}
				case 'x':
					/* Skipped, not stored. */
					goto NoOutput;
				case 'X':
					if( iPos < iSize ){
						iPos = -iSize;
						i = iRepeat - 1;
						if( iRepeat >= 0 ){
							ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
								"Type %c: outside of string",code);
						}
					}
					goto NoOutput;
				case '@':
					if( iRepeat <= nIn ){
						iPos = iRepeat;
					}else{
						ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
							"Type %c: outside of string",code);
					}
					i = iRepeat - 1;   /* php: one @ per entry, whatever it said */
					goto NoOutput;
				default: {
					int nWidth = (int)iSize;
					if( code == 'f' || code == 'g' || code == 'G' ){
						ph7_value_double(pVal,(double)PackGetFloat(&zIn[iPos],iOrder));
					}else if( code == 'd' || code == 'e' || code == 'E' ){
						ph7_value_double(pVal,PackGetDouble(&zIn[iPos],iOrder));
					}else{
						sxu64 uRaw = PackGetInt(&zIn[iPos],nWidth,iOrder);
						sxi64 iOut;
						/* Only the three SIGNED codes sign-extend; every other
						 * width answers the unsigned value it read, which is why
						 * `Q`/`J`/`P` of 0xFF... is -1 and `N` of the same four
						 * bytes is 4294967295. */
						if( code == 's' ){
							iOut = (sxi64)(sxi16)(sxu16)uRaw;
						}else if( code == 'i' ){
							iOut = (sxi64)(int)(unsigned int)uRaw;
						}else if( code == 'l' ){
							iOut = (sxi64)(sxi32)(sxu32)uRaw;
						}else{
							iOut = (sxi64)uRaw;
						}
						ph7_value_int64(pVal,iOut);
					}
					break;
				}
			}
			rc = UnpackStore(pCtx,pArray,pKey,pVal,zName,nName,i + 1,bIndexed);
			if( rc != PH7_OK ){
				return rc;
			}
NoOutput:
			iPos += iSize;
			if( iPos < 0 ){
				if( iSize != -1 ){
					/* An `X` says so through its own branch above; this is the
					 * cursor landing before the start any other way. */
					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
						"Type %c: outside of string",code);
				}
				iPos = 0;
			}
		}
		if( zCur < zFmtEnd ){
			zCur++;   /* step over the '/' separator */
		}
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
#endif /* PH7_NEED_BUILTIN_REG */
