/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxset.h"
#include "sxmem.h"
#include "sxhash.h"
#include "sxlex.h"
#include "sxbase64.h"
#include "sxuri.h"
#include "sxtime.h"
#include "sxstr.h"

PH7_PRIVATE sxu32 SyBinHash(const void *pSrc,sxu32 nLen)
{
	register unsigned char *zIn = (unsigned char *)pSrc;
	unsigned char *zEnd;
	sxu32 nH = 5381;
	zEnd = &zIn[nLen];
	for(;;){
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
	}
	return nH;
}
PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen)
{
	register unsigned char *zIn = (unsigned char *)pSrc;
	unsigned char *zEnd;
	sxu32 nH = 5381;
	zEnd = &zIn[nLen];
	for(;;){
		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + SyToLower(zIn[0]); zIn++;
	}
	return nH;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)
{
	static const unsigned char zBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	unsigned char *zIn = (unsigned char *)zSrc;
	unsigned char z64[4];
	sxu32 i;
	sxi32 rc;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) || xConsumer == 0){
		return SXERR_EMPTY;
	}
#endif
	for(i = 0; i + 2 < nLen; i += 3){
		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];
		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   | (zIn[i+1] >> 4)) & 0x3F];
		z64[2] = zBase64[( ((zIn[i+1] & 0x0F) << 2) | (zIn[i + 2] >> 6) ) & 0x3F];
		z64[3] = zBase64[ zIn[i + 2] & 0x3F];

		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);
		if( rc != SXRET_OK ){return SXERR_ABORT;}

	}
	if ( i+1 < nLen ){
		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];
		z64[1] = zBase64[( ((zIn[i] & 0x03) << 4)   | (zIn[i+1] >> 4)) & 0x3F];
		z64[2] = zBase64[(zIn[i+1] & 0x0F) << 2 ];
		z64[3] = '=';

		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);
		if( rc != SXRET_OK ){return SXERR_ABORT;}

	}else if( i < nLen ){
		z64[0] = zBase64[(zIn[i] >> 2) & 0x3F];
		z64[1]   = zBase64[(zIn[i] & 0x03) << 4];
		z64[2] = '=';
		z64[3] = '=';

		rc = xConsumer((const void *)z64,sizeof(z64),pUserData);
		if( rc != SXRET_OK ){return SXERR_ABORT;}
	}

	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)
{
	static const sxu32 aBase64Trans[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,0,0,1,2,3,4,
	5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,26,27,
	28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,0,0,
	0,0,0
	};
	sxu32 n,w,x,y,z;
	sxi32 rc;
	unsigned char zOut[10];
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zB64) || xConsumer == 0 ){
		return SXERR_EMPTY;
	}
#endif
	while(nLen > 0 && zB64[nLen - 1] == '=' ){
		nLen--;
	}
	for( n = 0 ; n+3<nLen ; n += 4){
		w = aBase64Trans[zB64[n] & 0x7F];
		x = aBase64Trans[zB64[n+1] & 0x7F];
		y = aBase64Trans[zB64[n+2] & 0x7F];
		z = aBase64Trans[zB64[n+3] & 0x7F];
		zOut[0] = ((w<<2) & 0xFC) | ((x>>4) & 0x03);
		zOut[1] = ((x<<4) & 0xF0) | ((y>>2) & 0x0F);
		zOut[2] = ((y<<6) & 0xC0) | (z & 0x3F);

		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*3,pUserData);
		if( rc != SXRET_OK ){ return SXERR_ABORT;}
	}
	if( n+2 < nLen ){
		w = aBase64Trans[zB64[n] & 0x7F];
		x = aBase64Trans[zB64[n+1] & 0x7F];
		y = aBase64Trans[zB64[n+2] & 0x7F];

		zOut[0] = ((w<<2) & 0xFC) | ((x>>4) & 0x03);
		zOut[1] = ((x<<4) & 0xF0) | ((y>>2) & 0x0F);

		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*2,pUserData);
		if( rc != SXRET_OK ){ return SXERR_ABORT;}
	}else if( n+1 < nLen ){
		w = aBase64Trans[zB64[n] & 0x7F];
		x = aBase64Trans[zB64[n+1] & 0x7F];

		zOut[0] = ((w<<2) & 0xFC) | ((x>>4) & 0x03);

		rc = xConsumer((const void *)zOut,sizeof(unsigned char)*1,pUserData);
		if( rc != SXRET_OK ){ return SXERR_ABORT;}
	}
	return SXRET_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#define INVALID_LEXER(LEX)	(  LEX == 0  || LEX->xTokenizer == 0 )
PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData)
{
	SyStream *pStream;
#if defined (UNTRUST)
	if ( pLex == 0 || xTokenizer == 0 ){
		return SXERR_CORRUPT;
	}
#endif
	pLex->pTokenSet = 0;
	/* Initialize lexer fields */
	if( pSet ){
		if ( SySetElemSize(pSet) != sizeof(SyToken) ){
			return SXERR_INVALID;
		}
		pLex->pTokenSet = pSet;
	}
	pStream = &pLex->sStream;
	pLex->xTokenizer = xTokenizer;
	pLex->pUserData = pUserData;

	pStream->nLine = 1;
	pStream->nIgn  = 0;
	pStream->zText = pStream->zEnd = 0;
	pStream->pSet  = pSet;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp)
{
	const unsigned char *zCur;
	SyStream *pStream;
	SyToken sToken;
	sxi32 rc;
#if defined (UNTRUST)
	if ( INVALID_LEXER(pLex) || zInput == 0 ){
		return SXERR_CORRUPT;
	}
#endif
	pStream = &pLex->sStream;
	/* Point to the head of the input */
	pStream->zText = pStream->zInput = (const unsigned char *)zInput;
	/* Point to the end of the input */
	pStream->zEnd = &pStream->zInput[nLen];
	for(;;){
		if( pStream->zText >= pStream->zEnd ){
			/* End of the input reached */
			break;
		}
		zCur = pStream->zText;
		/* Call the tokenizer callback */
		rc = pLex->xTokenizer(pStream,&sToken,pLex->pUserData,pCtxData);
		if( rc != SXRET_OK && rc != SXERR_CONTINUE ){
			/* Tokenizer callback request an operation abort */
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
		if( rc == SXERR_CONTINUE ){
			/* Request to ignore this token */
			pStream->nIgn++;
		}else if( pLex->pTokenSet  ){
			/* Put the token in the set */
			rc = SySetPut(pLex->pTokenSet,(const void *)&sToken);
			if( rc != SXRET_OK ){
				break;
			}
		}
		if( zCur >= pStream->zText ){
			/* Automatic advance of the stream cursor */
			pStream->zText = &zCur[1];
		}
	}
	if( xSort &&  pLex->pTokenSet ){
		SyToken *aToken = (SyToken *)SySetBasePtr(pLex->pTokenSet);
		/* Sort the extracted tokens */
		if( xCmp == 0 ){
			/* Use a default comparison function */
			xCmp = SyMemcmp;
		}
		xSort(aToken,SySetUsed(pLex->pTokenSet),sizeof(SyToken),xCmp);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex)
{
	sxi32 rc = SXRET_OK;
#if defined (UNTRUST)
	if ( INVALID_LEXER(pLex) ){
		return SXERR_CORRUPT;
	}
#else
	SXUNUSED(pLex); /* Prevent compiler warning */
#endif
	return rc;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define SAFE_HTTP(C)	(SyisAlphaNum(c) || c == '_' || c == '-' || c == '$' || c == '.' )
PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData)
{
	unsigned char *zIn = (unsigned char *)zSrc;
	unsigned char zHex[3] = { '%',0,0 };
	unsigned char zOut[2];
	unsigned char *zCur,*zEnd;
	sxi32 c;
	sxi32 rc;
#ifdef UNTRUST
	if( SX_EMPTY_STR(zSrc) || xConsumer == 0 ){
		return SXERR_EMPTY;
	}
#endif
	rc = SXRET_OK;
	zEnd = &zIn[nLen]; zCur = zIn;
	for(;;){
		if( zCur >= zEnd ){
			if( zCur != zIn ){
				rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData);
			}
			break;
		}
		c = zCur[0];
		if( SAFE_HTTP(c) ){
			zCur++; continue;
		}
		if( zCur != zIn && SXRET_OK != (rc = xConsumer(zIn,(sxu32)(zCur-zIn),pUserData))){
			break;
		}
		if( c == ' ' ){
			zOut[0] = '+';
			rc = xConsumer((const void *)zOut,sizeof(unsigned char),pUserData);
		}else{
			zHex[1]	= "0123456789ABCDEF"[(c >> 4) & 0x0F];
			zHex[2] = "0123456789ABCDEF"[c & 0x0F];
			rc = xConsumer(zHex,sizeof(zHex),pUserData);
		}
		if( SXRET_OK != rc ){
			break;
		}
		zIn = &zCur[1]; zCur = zIn ;
	}
	return rc == SXRET_OK ? SXRET_OK : SXERR_ABORT;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
static sxi32 SyAsciiToHex(sxi32 c)
{
	if( c >= 'a' && c <= 'f' ){
		c += 10 - 'a';
		return c;
	}
	if( c >= '0' && c <= '9' ){
		c -= '0';
		return c;
	}
	if( c >= 'A' && c <= 'F') {
		c += 10 - 'A';
		return c;
	}
	return 0;
}
PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8)
{
	static const sxu8 Utf8Trans[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00
	};
	const char *zIn = zSrc;
	const char *zEnd;
	const char *zCur;
	sxu8 *zOutPtr;
	sxu8 zOut[10];
	sxi32 c,d;
	sxi32 rc;
#if defined(UNTRUST)
	if( SX_EMPTY_STR(zSrc) || xConsumer == 0 ){
		return SXERR_EMPTY;
	}
#endif
	rc = SXRET_OK;
	zEnd = &zSrc[nLen];
	zCur = zIn;
	for(;;){
		while(zCur < zEnd && zCur[0] != '%' && zCur[0] != '+' ){
			zCur++;
		}
		if( zCur != zIn ){
			/* Consume input */
			rc = xConsumer(zIn,(unsigned int)(zCur-zIn),pUserData);
			if( rc != SXRET_OK ){
				/* User consumer routine request an operation abort */
				break;
			}
		}
		if( zCur >= zEnd ){
			rc = SXRET_OK;
			break;
		}
		/* Decode unsafe HTTP characters */
		zOutPtr = zOut;
		if( zCur[0] == '+' ){
			*zOutPtr++ = ' ';
			zCur++;
		}else{
			if( &zCur[2] >= zEnd ){
				rc = SXERR_OVERFLOW;
				break;
			}
			c = (SyAsciiToHex(zCur[1]) <<4) | SyAsciiToHex(zCur[2]);
			zCur += 3;
			if( c < 0x000C0 ){
				*zOutPtr++ = (sxu8)c;
			}else{
				c = Utf8Trans[c-0xC0];
				while( zCur[0] == '%' ){
					d = (SyAsciiToHex(zCur[1]) <<4) | SyAsciiToHex(zCur[2]);
					if( (d&0xC0) != 0x80 ){
						break;
					}
					c = (c<<6) + (0x3f & d);
					zCur += 3;
				}
				if( bUTF8 == FALSE ){
					*zOutPtr++ = (sxu8)c;
				}else{
					SX_WRITE_UTF8(zOutPtr,c);
				}
			}

		}
		/* Consume the decoded characters */
		rc = xConsumer((const void *)zOut,(unsigned int)(zOutPtr-zOut),pUserData);
		if( rc != SXRET_OK ){
			break;
		}
		/* Synchronize pointers */
		zIn = zCur;
	}
	return rc;
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
static const char *zEngDay[] = {
	"Sunday","Monday","Tuesday","Wednesday",
	"Thursday","Friday","Saturday"
};
static const char *zEngMonth[] = {
	"January","February","March","April",
	"May","June","July","August",
	"September","October","November","December"
};
static const char * GetDay(sxi32 i)
{
	return zEngDay[ i % 7 ];
}
static const char * GetMonth(sxi32 i)
{
	return zEngMonth[ i % 12 ];
}
PH7_PRIVATE const char * SyTimeGetDay(sxi32 iDay)
{
	return GetDay(iDay);
}
PH7_PRIVATE const char * SyTimeGetMonth(sxi32 iMonth)
{
	return GetMonth(iMonth);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
