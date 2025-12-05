/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxstr.h"

PH7_PRIVATE sxu32 SyStrlen(const char *zSrc)
{
	register const char *zIn = zSrc;
#if defined(UNTRUST)
	if( zIn == 0 ){
		return 0;
	}
#endif
	for(;;){
		if( !zIn[0] ){ break; } zIn++;
		if( !zIn[0] ){ break; } zIn++;
		if( !zIn[0] ){ break; } zIn++;
		if( !zIn[0] ){ break; } zIn++;
	}
	return (sxu32)(zIn - zSrc);
}
PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)
{
	const char *zIn = zStr;
	const char *zEnd;

	zEnd = &zIn[nLen];
	for(;;){
		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;
		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;
		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;
		if( zIn >= zEnd ){ break; }if( zIn[0] == c ){ if( pPos ){ *pPos = (sxu32)(zIn - zStr); } return SXRET_OK; } zIn++;
	}
	return SXERR_NOTFOUND;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos)
{
	const char *zIn = zStr;
	const char *zEnd;

	zEnd = &zIn[nLen - 1];
	for( ;; ){
		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;
		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;
		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;
		if( zEnd < zIn ){ break; } if( zEnd[0] == c ){ if( pPos ){ *pPos =  (sxu32)(zEnd - zIn);} return SXRET_OK; } zEnd--;
	}
	return SXERR_NOTFOUND;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos)
{
	const char *zIn = zSrc;
	const char *zPtr;
	const char *zEnd;
	sxi32 c;
	zEnd = &zSrc[nLen];
	for(;;){
		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;
		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;
		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;
		if( zIn >= zEnd ){ break; }	for(zPtr = zList ; (c = zPtr[0]) != 0 ; zPtr++ ){ if( zIn[0] == c ){ if( pFirstPos ){ *pFirstPos = (sxu32)(zIn - zSrc); } return SXRET_OK; } } zIn++;
	}
	return SXERR_NOTFOUND;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen)
{
	const unsigned char *zP = (const unsigned char *)zLeft;
	const unsigned char *zQ = (const unsigned char *)zRight;

	if( SX_EMPTY_STR(zP) || SX_EMPTY_STR(zQ)  ){
			return SX_EMPTY_STR(zP) ? (SX_EMPTY_STR(zQ) ? 0 : -1) :1;
	}
	if( nLen <= 0 ){
		return 0;
	}
	for(;;){
		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;
		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;
		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;
		if( nLen <= 0 ){ return 0; } if( zP[0] == 0 || zQ[0] == 0 || zP[0] != zQ[0] ){ break; } zP++; zQ++; nLen--;
	}
	return (sxi32)(zP[0] - zQ[0]);
}
#endif
PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft, const char *zRight,sxu32 SLen)
{
  	register unsigned char *p = (unsigned char *)zLeft;
	register unsigned char *q = (unsigned char *)zRight;

	if( SX_EMPTY_STR(p) || SX_EMPTY_STR(q) ){
		return SX_EMPTY_STR(p)? SX_EMPTY_STR(q) ? 0 : -1 :1;
	}
	for(;;){
		if( !SLen ){ return 0; }if( !*p || !*q || SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;
		if( !SLen ){ return 0; }if( !*p || !*q || SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;
		if( !SLen ){ return 0; }if( !*p || !*q || SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;
		if( !SLen ){ return 0; }if( !*p || !*q || SyCharToLower(*p) != SyCharToLower(*q) ){ break; }p++;q++;--SLen;

	}
	return (sxi32)(SyCharToLower(p[0]) - SyCharToLower(q[0]));
}
PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen)
{
	return SyStrnicmp((const char *)pLeft,(const char *)pRight,SLen);
}
PH7_PRIVATE sxu32 Systrcpy(char *zDest,sxu32 nDestLen,const char *zSrc,sxu32 nLen)
{
	unsigned char *zBuf = (unsigned char *)zDest;
	unsigned char *zIn = (unsigned char *)zSrc;
	unsigned char *zEnd;
#if defined(UNTRUST)
	if( zSrc == (const char *)zDest ){
			return 0;
	}
#endif
	if( nLen <= 0 ){
		nLen = SyStrlen(zSrc);
	}
	zEnd = &zBuf[nDestLen - 1]; /* reserve a room for the null terminator */
	for(;;){
		if( zBuf >= zEnd || nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;
		if( zBuf >= zEnd || nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;
		if( zBuf >= zEnd || nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;
		if( zBuf >= zEnd || nLen == 0 ){ break;} zBuf[0] = zIn[0]; zIn++; zBuf++; nLen--;
	}
	zBuf[0] = 0;
	return (sxu32)(zBuf-(unsigned char *)zDest);
}
