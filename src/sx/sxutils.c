/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxutils.h"
#include "sxstr.h"
#include <stdlib.h> /* strtod — SyStrToReal must be correctly rounded (see its comment) */

PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char  **pzTail)
{
	const char *zCur,*zEnd;
#ifdef UNTRUST
	if( SX_EMPTY_STR(zSrc) ){
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	/* Jump leading white spaces */
	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0  && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-') ){
		zSrc++;
	}
	zCur = zSrc;
	if( pReal ){
		*pReal = FALSE;
	}
	for(;;){
		if( zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]) ){
			break;
		}
		zSrc++;
		if( zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]) ){
			break;
		}
		zSrc++;
		if( zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]) ){
			break;
		}
		zSrc++;
		if( zSrc >= zEnd || (unsigned char)zSrc[0] >= 0xc0 || !SyisDigit(zSrc[0]) ){
			break;
		}
		zSrc++;
	};
	if( zSrc < zEnd && zSrc > zCur ){
		int c = zSrc[0];
		if( c == '.' ){
			zSrc++;
			if( pReal ){
				*pReal = TRUE;
			}
			if( pzTail ){
				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){
					zSrc++;
				}
				if( zSrc < zEnd && (zSrc[0] == 'e' || zSrc[0] == 'E') ){
					zSrc++;
					if( zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-') ){
						zSrc++;
					}
					while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){
						zSrc++;
					}
				}
			}
		}else if( c == 'e' || c == 'E' ){
			zSrc++;
			if( pReal ){
				*pReal = TRUE;
			}
			if( pzTail ){
				if( zSrc < zEnd && (zSrc[0] == '+' || zSrc[0] == '-') ){
					zSrc++;
				}
				while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){
					zSrc++;
				}
			}
		}
	}
	if( pzTail ){
		/* Point to the non numeric part */
		*pzTail = zSrc;
	}
	return zSrc > zCur ? SXRET_OK /* String prefix is numeric */ : SXERR_INVALID /* Not a digit stream */;
}
#define SXINT32_MIN_STR		"2147483648"
#define SXINT32_MAX_STR		"2147483647"
PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	int isNeg = FALSE;
	const char *zEnd;
	sxi32 nVal = 0;
	sxi16 i;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) ){
		if( pOutVal ){
			*(sxi32 *)pOutVal = 0;
		}
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && ( zSrc[0] == '-' || zSrc[0] == '+' ) ){
		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;
		zSrc++;
	}
	/* Skip leading zero */
	while(zSrc < zEnd && zSrc[0] == '0' ){
		zSrc++;
	}
	i = 10;
	if( (sxu32)(zEnd-zSrc) >= 10 ){
		/* Handle overflow */
		i = SyMemcmp(zSrc,(isNeg == TRUE) ? SXINT32_MIN_STR : SXINT32_MAX_STR,nLen) <= 0 ? 10 : 9;
	}
	for(;;){
		if(zSrc >= zEnd || !i || !SyisDigit(zSrc[0])){ break; } nVal = nVal * 10 + ( zSrc[0] - '0' ) ; --i ; zSrc++;
		if(zSrc >= zEnd || !i || !SyisDigit(zSrc[0])){
			break;
		}
		nVal = nVal * 10 + ( zSrc[0] - '0' );
		--i;
		zSrc++;
		if(zSrc >= zEnd || !i || !SyisDigit(zSrc[0])){
			break;
		}
		nVal = nVal * 10 + ( zSrc[0] - '0' );
		--i;
		zSrc++;
		if(zSrc >= zEnd || !i || !SyisDigit(zSrc[0])){
			break;
		}
		nVal = nVal * 10 + ( zSrc[0] - '0' );
		--i;
		zSrc++;
		if(zSrc >= zEnd || !i || !SyisDigit(zSrc[0])){
			break;
		}
		nVal = nVal * 10 + ( zSrc[0] - '0' );
		--i;
		zSrc++;
	}
	/* Skip trailing spaces */
	while(zSrc < zEnd && SyisSpace(zSrc[0])){
		zSrc++;
	}
	if( zRest ){
		*zRest = (char *)zSrc;
	}
	if( pOutVal ){
		if( isNeg == TRUE && nVal != 0 ){
			nVal = -nVal;
		}
		*(sxi32 *)pOutVal = nVal;
	}
	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	int isNeg = FALSE;
	const char *zEnd;
	sxi64 nVal;
	/* Magnitude accumulated unsigned so overflow can be detected and the result
	 * saturated (PHP casts an out-of-range numeric string to PHP_INT_MAX/MIN)
	 * rather than the digits being dropped. cutoff is the largest magnitude that
	 * fits: PHP_INT_MAX for a positive value, |PHP_INT_MIN| == 2^63 for a
	 * negative one. */
	sxu64 uVal, cutoff;
	int bOverflow = FALSE;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) ){
		if( pOutVal ){
			*(sxi32 *)pOutVal = 0;
		}
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && ( zSrc[0] == '-' || zSrc[0] == '+' ) ){
		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;
		zSrc++;
	}
	/* Skip leading zero */
	while(zSrc < zEnd && zSrc[0] == '0' ){
		zSrc++;
	}
	cutoff = isNeg ? ((sxu64)SXI64_HIGH + 1) : (sxu64)SXI64_HIGH;
	uVal = 0;
	while( zSrc < zEnd && (unsigned char)zSrc[0] < 0xc0 && SyisDigit(zSrc[0]) ){
		int d = zSrc[0] - '0';
		if( uVal > cutoff / 10 || (uVal == cutoff / 10 && (sxu64)d > cutoff % 10) ){
			bOverflow = TRUE;
		}else{
			uVal = uVal * 10 + (sxu64)d;
		}
		zSrc++;
	}
	if( bOverflow ){
		uVal = cutoff;
	}
	/* Skip trailing spaces */
	while(zSrc < zEnd && SyisSpace(zSrc[0])){
		zSrc++;
	}
	if( zRest ){
		*zRest = (char *)zSrc;
	}
	if( pOutVal ){
		if( isNeg ){
			/* uVal <= 2^63; the cap value 2^63 is PHP_INT_MIN and has no positive
			 * sxi64 representation, so materialize it directly to dodge UB. */
			nVal = ( uVal > (sxu64)SXI64_HIGH ) ? (-SXI64_HIGH - 1) : -(sxi64)uVal;
		}else{
			nVal = (sxi64)uVal;
		}
		*(sxi64 *)pOutVal = nVal;
	}
	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 SyHexToint(sxi32 c)
{
	switch(c){
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A': case 'a': return 10;
	case 'B': case 'b': return 11;
	case 'C': case 'c': return 12;
	case 'D': case 'd': return 13;
	case 'E': case 'e': return 14;
	case 'F': case 'f': return 15;
	}
	return -1;
}
PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	const char *zIn,*zEnd;
	int isNeg = FALSE;
	sxi64 nVal = 0;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) ){
		if( pOutVal ){
			*(sxi32 *)pOutVal = 0;
		}
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && ( *zSrc == '-' || *zSrc == '+' ) ){
		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;
		zSrc++;
	}
	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'x' || zSrc[1] == 'X') ){
		/* Bypass hex prefix */
		zSrc += sizeof(char) * 2;
	}
	/* Skip leading zero */
	while(zSrc < zEnd && zSrc[0] == '0' ){
		zSrc++;
	}
	zIn = zSrc;
	for(;;){
		if(zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc-zIn) > 15){
			break;
		}
		nVal = nVal * 16 + SyHexToint(zSrc[0]);
		zSrc++;
		if(zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc-zIn) > 15){
			break;
		}
		nVal = nVal * 16 + SyHexToint(zSrc[0]);
		zSrc++;
		if(zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc-zIn) > 15){
			break;
		}
		nVal = nVal * 16 + SyHexToint(zSrc[0]);
		zSrc++;
		if(zSrc >= zEnd || !SyisHex(zSrc[0]) || (int)(zSrc-zIn) > 15){
			break;
		}
		nVal = nVal * 16 + SyHexToint(zSrc[0]);
		zSrc++;
	}
	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zRest ){
		*zRest = zSrc;
	}
	if( pOutVal ){
		if( isNeg == TRUE && nVal != 0 ){
			nVal = -nVal;
		}
		*(sxi64 *)pOutVal = nVal;
	}
	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	const char *zIn,*zEnd;
	int isNeg = FALSE;
	sxi64 nVal = 0;
	int c;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) ){
		if( pOutVal ){
			*(sxi32 *)pOutVal = 0;
		}
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && ( zSrc[0] == '-' || zSrc[0] == '+' ) ){
		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;
		zSrc++;
	}
	/* Skip leading zero */
	while(zSrc < zEnd && zSrc[0] == '0' ){
		zSrc++;
	}
	zIn = zSrc;
	for(;;){
		if(zSrc >= zEnd || !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 || (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;
		if(zSrc >= zEnd || !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 || (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;
		if(zSrc >= zEnd || !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 || (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;
		if(zSrc >= zEnd || !SyisDigit(zSrc[0])){ break; } if( (c=zSrc[0]-'0') > 7 || (int)(zSrc-zIn) > 20){ break;} nVal = nVal * 8 +  c; zSrc++;
	}
	/* Skip trailing spaces */
	while(zSrc < zEnd && SyisSpace(zSrc[0])){
		zSrc++;
	}
	if( zRest ){
		*zRest = zSrc;
	}
	if( pOutVal ){
		if( isNeg == TRUE && nVal != 0 ){
			nVal = -nVal;
		}
		*(sxi64 *)pOutVal = nVal;
	}
	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	const char *zIn,*zEnd;
	int isNeg = FALSE;
	sxi64 nVal = 0;
	int c;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) ){
		if( pOutVal ){
			*(sxi32 *)pOutVal = 0;
		}
		return SXERR_EMPTY;
	}
#endif
	zEnd = &zSrc[nLen];
	while(zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zSrc < zEnd && ( zSrc[0] == '-' || zSrc[0] == '+' ) ){
		isNeg = (zSrc[0] == '-') ? TRUE :FALSE;
		zSrc++;
	}
	if( zSrc < &zEnd[-2] && zSrc[0] == '0' && (zSrc[1] == 'b' || zSrc[1] == 'B') ){
		/* Bypass binary prefix */
		zSrc += sizeof(char) * 2;
	}
	/* Skip leading zero */
	while(zSrc < zEnd && zSrc[0] == '0' ){
		zSrc++;
	}
	zIn = zSrc;
	for(;;){
		if(zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;
		if(zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;
		if(zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;
		if(zSrc >= zEnd || (zSrc[0] != '1' && zSrc[0] != '0') || (int)(zSrc-zIn) > 62){ break; } c = zSrc[0] - '0'; nVal = (nVal << 1) + c; zSrc++;
	}
	/* Skip trailing spaces */
	while(zSrc < zEnd && SyisSpace(zSrc[0])){
		zSrc++;
	}
	if( zRest ){
		*zRest = zSrc;
	}
	if( pOutVal ){
		if( isNeg == TRUE && nVal != 0 ){
			nVal = -nVal;
		}
		*(sxi64 *)pOutVal = nVal;
	}
	return (zSrc >= zEnd) ? SXRET_OK : SXERR_SYNTAX;
}
PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void * pOutVal,const char **zRest)
{
	/* Correctly-rounded conversion via libc strtod (the byte-exact-floats
	 * rule): the old hand-rolled accumulator kept only 15 significant
	 * digits, clamped exponents to +/-30x (so 1e400 silently became 1e308
	 * and 5e-324 became 5e-307) and drifted the low mantissa bits, making
	 * float literals and string->float casts differ from php. The accepted
	 * numeric-prefix grammar is kept from the old parser, including the ','
	 * decimal separator: [ws][sign]D*[(.|,)D*][(e|E)[sign]D+][ws]. The
	 * prefix is copied (',' -> '.') into a stack buffer for strtod — or a
	 * heap copy for the rare number longer than the buffer (e.g. hundreds of
	 * leading fractional zeros before the significant digits), falling back
	 * to a truncated-mantissa-plus-exponent copy only if that allocation
	 * fails. Everything is always consumed from the input. */
	char zBuf[512];
	const char *zEnd = &zSrc[nLen];
	const char *zNum;
	const char *zExpStart = 0;
	sxreal Val = 0.0;
	sxu32 nCopy = 0;
	int bDigit = 0;
#ifdef UNTRUST
	if( SX_EMPTY_STR(zSrc)  ){
		if( pOutVal ){
			*(sxreal *)pOutVal = 0.0;
		}
		return SXERR_EMPTY;
	}
#endif
	/* Skip leading spaces */
	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	zNum = zSrc;
	/* Sign (if exists) */
	if( zSrc < zEnd && (zSrc[0] == '-' || zSrc[0] == '+' ) ){
		zSrc++;
	}
	/* Integer part */
	while( zSrc < zEnd && SyisDigit(zSrc[0]) ){
		bDigit = 1;
		zSrc++;
	}
	/* Fractional part */
	if( zSrc < zEnd && ( zSrc[0] == '.' || zSrc[0] == ',' ) ){
		zSrc++;
		while( zSrc < zEnd && SyisDigit(zSrc[0]) ){
			bDigit = 1;
			zSrc++;
		}
	}
	/* Exponent — consumed only when it carries at least one digit, like
	 * strtod, so "1e+x" leaves the "e+" unconsumed. */
	if( bDigit && zSrc < zEnd && ( zSrc[0] == 'e' || zSrc[0] == 'E' ) ){
		const char *zExp = &zSrc[1];
		if( zExp < zEnd && (zExp[0] == '-' || zExp[0] == '+') ){
			zExp++;
		}
		if( zExp < zEnd && SyisDigit(zExp[0]) ){
			zExpStart = zSrc;
			zSrc = zExp;
			while( zSrc < zEnd && SyisDigit(zSrc[0]) ){
				zSrc++;
			}
		}
	}
	if( bDigit ){
		sxu32 i, nSpan = (sxu32)((zExpStart ? zExpStart : zSrc) - zNum);
		sxu32 nExp = zExpStart ? (sxu32)(zSrc - zExpStart) : 0;
		char *zDup = zBuf;
		sxu32 nDup = sizeof(zBuf);
		if( nSpan + nExp >= sizeof(zBuf) ){
			char *zHeap = (char *)malloc(nSpan + nExp + 1);
			if( zHeap ){
				zDup = zHeap;
				nDup = nSpan + nExp + 1;
			}
		}
		{
			sxu32 nMantMax = nDup - 1 - (nExp < nDup - 1 ? nExp : 0);
			for( i = 0 ; i < nSpan && nCopy < nMantMax ; i++ ){
				zDup[nCopy++] = (zNum[i] == ',') ? '.' : zNum[i];
			}
			/* The exponent rides behind even a truncated mantissa: dropping
			 * it would collapse "0.<hundreds of zeros>1e300" to 0.0. */
			for( i = 0 ; i < nExp && nCopy < nDup - 1 ; i++ ){
				zDup[nCopy++] = zExpStart[i];
			}
		}
		zDup[nCopy] = 0;
		Val = (sxreal)strtod(zDup,0);
		if( zDup != zBuf ){
			free(zDup);
		}
	}
	/* Jump trailing spaces */
	while( zSrc < zEnd && SyisSpace(zSrc[0]) ){
		zSrc++;
	}
	if( zRest ){
		*zRest = zSrc;
	}
	if( pOutVal ){
		*(sxreal *)pOutVal = Val;
	}
	return zSrc >= zEnd ? SXRET_OK : SXERR_SYNTAX;
}
