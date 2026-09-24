/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <string.h>

#ifndef PH7_DISABLE_DISK_IO
/*
 * Section:
 *    Stream filters — php's stream_filter_* family and the chains it runs.
 * Status:
 *    Stable.
 *
 * A php stream carries two chains of filters: everything that comes off the
 * device is run through the READ chain before the script sees a byte of it, and
 * everything the script writes is run through the WRITE chain before the device
 * does. Neither is a byte-for-byte mapping — base64 makes four bytes out of
 * three and dechunk throws whole runs away — which is why a filtered read
 * cannot be served straight into the caller's buffer and why the chain speaks
 * in BRIGADES: a filter takes the buckets that arrived and appends what it made
 * to a second brigade, and what it ANSWERS says whether that output may go on
 * (PASS_ON), whether it needs more input before it can produce any (FEED_ME) or
 * whether the stream is finished (ERR_FATAL).
 *
 * Every chain call carries a FLAG saying which kind of call it is, and
 * FLUSH_CLOSE — the one a filter gets when the device hit its end, when the
 * handle is closed and when the filter is removed — is the only chance a
 * buffering filter has to emit the tail it is holding.
 */
/* --------------------------------------------------------------------------
 * Brigades.
 * -------------------------------------------------------------------------- */
PH7_PRIVATE phl_bucket * PH7_FilterBucketNew(ph7_vm *pVm,const void *pData,sxu32 nLen)
{
	phl_bucket *pBucket;
	if( pVm == 0 ){
		return 0;
	}
	pBucket = (phl_bucket *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_bucket));
	if( pBucket == 0 ){
		return 0;
	}
	SyZero(pBucket,sizeof(phl_bucket));
	SyBlobInit(&pBucket->sData,&pVm->sAllocator);
	if( nLen > 0 && pData != 0 ){
		if( SyBlobAppend(&pBucket->sData,pData,nLen) != SXRET_OK ){
			SyBlobRelease(&pBucket->sData);
			SyMemBackendFree(&pVm->sAllocator,pBucket);
			return 0;
		}
	}
	return pBucket;
}
PH7_PRIVATE void PH7_FilterBucketAppend(phl_brigade *pBrig,phl_bucket *pBucket)
{
	if( pBucket == 0 ){
		return;
	}
	pBucket->pNext = 0;
	if( pBrig->pTail ){
		pBrig->pTail->pNext = pBucket;
	}else{
		pBrig->pHead = pBucket;
	}
	pBrig->pTail = pBucket;
}
PH7_PRIVATE void PH7_FilterBucketFree(ph7_vm *pVm,phl_bucket *pBucket)
{
	if( pBucket == 0 ){
		return;
	}
	SyBlobRelease(&pBucket->sData);
	SyMemBackendFree(&pVm->sAllocator,pBucket);
}
/* Unlink and answer the first bucket of a brigade, or 0 when it is empty. */
static phl_bucket * FilterBucketPop(phl_brigade *pBrig)
{
	phl_bucket *pBucket = pBrig->pHead;
	if( pBucket == 0 ){
		return 0;
	}
	pBrig->pHead = pBucket->pNext;
	if( pBrig->pHead == 0 ){
		pBrig->pTail = 0;
	}
	pBucket->pNext = 0;
	return pBucket;
}
PH7_PRIVATE void PH7_FilterBrigadeRelease(ph7_vm *pVm,phl_brigade *pBrig)
{
	phl_bucket *pBucket;
	while( (pBucket = FilterBucketPop(pBrig)) != 0 ){
		PH7_FilterBucketFree(pVm,pBucket);
	}
}
/* --------------------------------------------------------------------------
 * The built-in filters.
 * -------------------------------------------------------------------------- */
/* php's string.* trio: one byte in, one byte out, no state at all. */
#define PHL_STRF_ROT13   0
#define PHL_STRF_TOUPPER 1
#define PHL_STRF_TOLOWER 2
static int StringFilterRun(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,
	int iFlags,int iMode)
{
	phl_bucket *pBucket;
	SXUNUSED(iFlags);
	SXUNUSED(pFilter);
	while( (pBucket = FilterBucketPop(pIn)) != 0 ){
		unsigned char *zData = (unsigned char *)SyBlobData(&pBucket->sData);
		sxu32 n,nLen = SyBlobLength(&pBucket->sData);
		for( n = 0 ; n < nLen ; n++ ){
			int c = zData[n];
			if( iMode == PHL_STRF_TOUPPER ){
				if( c >= 'a' && c <= 'z' ){
					c -= 32;
				}
			}else if( iMode == PHL_STRF_TOLOWER ){
				if( c >= 'A' && c <= 'Z' ){
					c += 32;
				}
			}else{
				if( c >= 'a' && c <= 'z' ){
					c = 'a' + (c - 'a' + 13) % 26;
				}else if( c >= 'A' && c <= 'Z' ){
					c = 'A' + (c - 'A' + 13) % 26;
				}
			}
			zData[n] = (unsigned char)c;
		}
		PH7_FilterBucketAppend(pOut,pBucket);
	}
	return PHL_PSFS_PASS_ON;
}
static int Rot13Filter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_ROT13);
}
static int ToUpperFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOUPPER);
}
static int ToLowerFilter(phl_stream_filter *pF,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	return StringFilterRun(pF,pIn,pOut,iFlags,PHL_STRF_TOLOWER);
}
/* --------------------------------------------------------------------------
 * convert.* — the CODEC filters, and the shape they share.
 *
 * php registers ONE factory for the whole `convert.` prefix and picks the codec
 * from the rest of the name. All four are stateful across calls: a chunk
 * boundary can fall in the middle of a base64 group or of a `=XX` escape, so
 * what cannot be finished yet is carried to the next call and what is still
 * carried at the closing one is what the tail is made of.
 * -------------------------------------------------------------------------- */
#define PHL_CONV_B64_ENCODE 0
#define PHL_CONV_B64_DECODE 1
#define PHL_CONV_QP_ENCODE  2
#define PHL_CONV_QP_DECODE  3
typedef struct phl_conv_state phl_conv_state;
struct phl_conv_state
{
	int iKind;        /* PHL_CONV_* */
	int iLineLen;     /* `line-length`, 0 = never wrap */
	int iCol;         /* how much of the current output line is used */
	int bBinary;      /* quoted-printable's `binary`: space and tab encoded too */
	int bErr;         /* a byte sequence php refuses was seen */
	int bPadded;      /* base64-decode: the `=` arrived, the rest is ignored */
	int iMatch;       /* qp-encode: bytes of sBreak matched so far in the INPUT */
	SyBlob sBreak;    /* `line-break-chars` */
};
/* The `line-length`/`line-break-chars`/`binary` triple, read once. php refuses a
 * $params that is present and not an array — which an explicit NULL is, so
 * `stream_filter_append($h,'convert.base64-encode',STREAM_FILTER_READ,null)`
 * fails where omitting the argument works. */
static int ConvFilterCreate(phl_stream_filter *pFilter,ph7_value *pParams)
{
	phl_conv_state *pState;
	const char *zName = (const char *)SyBlobData(&pFilter->sName);
	int nName = (int)SyBlobLength(&pFilter->sName);
	int iKind;
	if( nName > 8 && SyMemcmp(zName,"convert.",8) == 0 ){
		const char *z = &zName[8];
		int n = nName - 8;
		if( n == (int)sizeof("base64-encode")-1 && SyMemcmp(z,"base64-encode",13) == 0 ){
			iKind = PHL_CONV_B64_ENCODE;
		}else if( n == (int)sizeof("base64-decode")-1 && SyMemcmp(z,"base64-decode",13) == 0 ){
			iKind = PHL_CONV_B64_DECODE;
		}else if( n == (int)sizeof("quoted-printable-encode")-1
		       && SyMemcmp(z,"quoted-printable-encode",23) == 0 ){
			iKind = PHL_CONV_QP_ENCODE;
		}else if( n == (int)sizeof("quoted-printable-decode")-1
		       && SyMemcmp(z,"quoted-printable-decode",23) == 0 ){
			iKind = PHL_CONV_QP_DECODE;
		}else{
			return -1; /* convert.<something this build has no codec for> */
		}
	}else{
		return -1;
	}
	if( pParams != 0 && !ph7_value_is_array(pParams) ){
		char zMsg[96];
		SyBufferFormat(zMsg,sizeof(zMsg),"Stream filter (%.*s): invalid filter parameter",
			nName,zName);
		PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);
		return -1;
	}
	pState = (phl_conv_state *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,sizeof(phl_conv_state));
	if( pState == 0 ){
		return -1;
	}
	SyZero(pState,sizeof(phl_conv_state));
	pState->iKind = iKind;
	SyBlobInit(&pState->sBreak,&pFilter->pVm->sAllocator);
	SyBlobAppend(&pState->sBreak,"\r\n",2); /* php's default */
	if( pParams ){
		ph7_value *pVal;
		pVal = ph7_array_fetch(pParams,"line-length",-1);
		if( pVal ){
			pState->iLineLen = (int)ph7_value_to_int(pVal);
		}
		pVal = ph7_array_fetch(pParams,"line-break-chars",-1);
		if( pVal ){
			int nLb;
			const char *zLb = ph7_value_to_string(pVal,&nLb);
			SyBlobReset(&pState->sBreak);
			if( nLb > 0 ){
				SyBlobAppend(&pState->sBreak,zLb,(sxu32)nLb);
			}
		}
		pVal = ph7_array_fetch(pParams,"binary",-1);
		if( pVal ){
			pState->bBinary = ph7_value_to_bool(pVal) != 0;
		}
	}
	/* php's wrapping thresholds, which are not the same on the two encoders:
	 * base64 rounds the length DOWN to a whole group of four and wraps only at
	 * four or more, and quoted-printable wants room for a `=XX` escape plus its
	 * own soft-break `=` — so anything under four never wraps at all. */
	if( iKind == PHL_CONV_B64_ENCODE ){
		pState->iLineLen = (pState->iLineLen / 4) * 4;
	}
	if( pState->iLineLen < 4 ){
		pState->iLineLen = 0;
	}
	pFilter->pPriv = (void *)pState;
	return PH7_OK;
}
static void ConvFilterClose(phl_stream_filter *pFilter)
{
	phl_conv_state *pState = (phl_conv_state *)pFilter->pPriv;
	if( pState ){
		SyBlobRelease(&pState->sBreak);
		SyMemBackendFree(&pFilter->pVm->sAllocator,pState);
		pFilter->pPriv = 0;
	}
}
/* Emit one output byte, wrapping when the codec asked for a line length. */
static void ConvEmit(phl_conv_state *pState,SyBlob *pOut,int c,int bSoftBreak)
{
	if( pState->iLineLen > 0 && SyBlobLength(&pState->sBreak) > 0 ){
		int nRoom = bSoftBreak ? pState->iLineLen - 1 : pState->iLineLen;
		if( pState->iCol >= nRoom ){
			if( bSoftBreak ){
				char eq = '=';
				SyBlobAppend(pOut,&eq,1);
			}
			SyBlobAppend(pOut,SyBlobData(&pState->sBreak),SyBlobLength(&pState->sBreak));
			pState->iCol = 0;
		}
	}
	{
		char ch = (char)c;
		SyBlobAppend(pOut,&ch,1);
	}
	pState->iCol++;
}
static const char zB64Alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
/* -1 for a byte base64 does not use; php SKIPS those rather than refusing. */
static int ConvB64Value(int c)
{
	const char *z = zB64Alpha;
	int i;
	for( i = 0 ; i < 64 ; i++ ){
		if( z[i] == c ){
			return i;
		}
	}
	return -1;
}
static void ConvB64EncodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,
	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut,int bClosing)
{
	sxu32 i;
	for( i = 0 ; i < nIn ; i++ ){
		char b = (char)zIn[i];
		SyBlobAppend(&pFilter->sCarry,&b,1);
		if( SyBlobLength(&pFilter->sCarry) == 3 ){
			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);
			ConvEmit(pState,pOut,zB64Alpha[z[0]>>2],0);
			ConvEmit(pState,pOut,zB64Alpha[((z[0]&0x03)<<4)|(z[1]>>4)],0);
			ConvEmit(pState,pOut,zB64Alpha[((z[1]&0x0F)<<2)|(z[2]>>6)],0);
			ConvEmit(pState,pOut,zB64Alpha[z[2]&0x3F],0);
			SyBlobReset(&pFilter->sCarry);
		}
	}
	if( bClosing && SyBlobLength(&pFilter->sCarry) > 0 ){
		const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);
		sxu32 n = SyBlobLength(&pFilter->sCarry);
		ConvEmit(pState,pOut,zB64Alpha[z[0]>>2],0);
		if( n == 1 ){
			ConvEmit(pState,pOut,zB64Alpha[(z[0]&0x03)<<4],0);
			ConvEmit(pState,pOut,'=',0);
		}else{
			ConvEmit(pState,pOut,zB64Alpha[((z[0]&0x03)<<4)|(z[1]>>4)],0);
			ConvEmit(pState,pOut,zB64Alpha[(z[1]&0x0F)<<2],0);
		}
		ConvEmit(pState,pOut,'=',0);
		SyBlobReset(&pFilter->sCarry);
	}
}
static void ConvB64DecodeTail(phl_stream_filter *pFilter,SyBlob *pOut);
static void ConvB64DecodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,
	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)
{
	sxu32 i;
	for( i = 0 ; i < nIn ; i++ ){
		int v;
		if( zIn[i] == '=' ){
			/* Padding. php refuses one that BEGINS a group — there is nothing for
			 * it to pad — and otherwise ends the stream. A second `=` is the rest
			 * of the same padding and never a new group. */
			if( pState->bPadded ){
				continue;
			}
			if( SyBlobLength(&pFilter->sCarry) == 0 ){
				pState->bErr = 1;
			}else{
				/* The padding CLOSES the group: what the carry holds is decoded
				 * now, not at the end of the stream — php hands those bytes to
				 * the reader before anything that follows can refuse. */
				ConvB64DecodeTail(pFilter,pOut);
			}
			pState->bPadded = 1;
			continue;
		}
		v = ConvB64Value(zIn[i]);
		if( pState->bPadded ){
			/* php ends the stream at the padding: another alphabet byte after it
			 * is an invalid sequence, not something to ignore. */
			if( v >= 0 ){
				pState->bErr = 1;
			}
			continue;
		}
		if( v < 0 ){
			continue; /* php SKIPS a byte outside the alphabet */
		}
		{
			char b = (char)v;
			SyBlobAppend(&pFilter->sCarry,&b,1);
		}
		if( SyBlobLength(&pFilter->sCarry) == 4 ){
			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);
			char zOut[3];
			zOut[0] = (char)((z[0]<<2)|(z[1]>>4));
			zOut[1] = (char)((z[1]<<4)|(z[2]>>2));
			zOut[2] = (char)((z[2]<<6)|z[3]);
			SyBlobAppend(pOut,zOut,3);
			SyBlobReset(&pFilter->sCarry);
		}
	}
}
/* What is left in the carry when the padding (or the stream) arrived. */
static void ConvB64DecodeTail(phl_stream_filter *pFilter,SyBlob *pOut)
{
	const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);
	sxu32 n = SyBlobLength(&pFilter->sCarry);
	char zOut[2];
	if( n >= 2 ){
		zOut[0] = (char)((z[0]<<2)|(z[1]>>4));
		if( n >= 3 ){
			zOut[1] = (char)((z[1]<<4)|(z[2]>>2));
			SyBlobAppend(pOut,zOut,2);
		}else{
			SyBlobAppend(pOut,zOut,1);
		}
	}
	SyBlobReset(&pFilter->sCarry);
}
/* php's printable set: everything but a byte that has to be escaped. Space and
 * tab are literal in TEXT mode and escaped in binary mode; the line break the
 * input carries is always escaped, since the only breaks php's encoder writes
 * are the SOFT ones it makes itself. */
static int ConvQpLiteral(phl_conv_state *pState,int c)
{
	if( c == '=' ){
		return 0;
	}
	if( c == ' ' || c == '\t' ){
		return !pState->bBinary;
	}
	return c >= 33 && c <= 126;
}
static void ConvQpEncodeOne(phl_conv_state *pState,int c,SyBlob *pOut);
/*
 * php only looks for lines once a line LENGTH is set: with no wrapping there
 * are no lines, so the configured break is not recognised in the input and a
 * space is just a space. With wrapping on, two more rules appear, and they do
 * NOT see the same distance.
 *
 * The input's own line break is a HARD one — written through untouched and
 * starting the column over — and php remembers a partial match ACROSS calls, so
 * a break split by a chunk boundary is still one.
 *
 * WHITESPACE is decided with what this call holds and nothing more: a space or
 * tab may be written literally only when the next byte is a non-whitespace one
 * with two more bytes behind it, because trailing whitespace is exactly what
 * quoted-printable must escape and php cannot see past the buffer it was given.
 * That is why the same input encodes DIFFERENTLY under a small chunk size —
 * `stream_set_chunk_size($h,1)` escapes every space — and matching php means
 * looking exactly as far as php does.
 */
static void ConvQpEncodeBytes(phl_conv_state *pState,const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)
{
	const unsigned char *zBreak = (const unsigned char *)SyBlobData(&pState->sBreak);
	int nBreak = (int)SyBlobLength(&pState->sBreak);
	/* BINARY mode has no lines at all: every byte that is not printable is
	 * escaped, the input's own line ending included. */
	int bLines = pState->iLineLen > 0 && nBreak > 0 && !pState->bBinary;
	sxu32 i;
	for( i = 0 ; i < nIn ; i++ ){
		int c = zIn[i];
		if( bLines ){
			if( c == zBreak[pState->iMatch] ){
				pState->iMatch++;
				if( pState->iMatch >= nBreak ){
					SyBlobAppend(pOut,zBreak,(sxu32)nBreak);
					pState->iCol = 0;
					pState->iMatch = 0;
				}
				continue;
			}
			if( pState->iMatch > 0 ){
				int k,nMatched = pState->iMatch;
				pState->iMatch = 0;
				for( k = 0 ; k < nMatched ; k++ ){
					ConvQpEncodeOne(pState,zBreak[k],pOut);
				}
				if( c == zBreak[0] ){
					pState->iMatch = 1;
					continue;
				}
			}
			if( c == ' ' || c == '\t' ){
				/* …and the whitespace that ENDS a line is the whole point of the
				 * escape, so a space right before the break is never literal. */
				int bLiteral = (i + 2 < nIn) && zIn[i+1] != ' ' && zIn[i+1] != '\t'
					&& zIn[i+1] != zBreak[0];
				if( bLiteral ){
					ConvEmit(pState,pOut,c,1);
				}else{
					pState->bBinary = 1;   /* escape it, just this once */
					ConvQpEncodeOne(pState,c,pOut);
					pState->bBinary = 0;
				}
				continue;
			}
		}
		ConvQpEncodeOne(pState,c,pOut);
	}
}
static void ConvQpEncodeOne(phl_conv_state *pState,int c,SyBlob *pOut)
{
	static const char zHex[] = "0123456789ABCDEF";
	{
		if( ConvQpLiteral(pState,c) ){
			ConvEmit(pState,pOut,c,1);
		}else{
			/* A three-byte escape is never split across a soft break. */
			if( pState->iLineLen > 0 && SyBlobLength(&pState->sBreak) > 0
			 && pState->iCol + 3 > pState->iLineLen - 1 ){
				char eq = '=';
				SyBlobAppend(pOut,&eq,1);
				SyBlobAppend(pOut,SyBlobData(&pState->sBreak),SyBlobLength(&pState->sBreak));
				pState->iCol = 0;
			}
			ConvEmit(pState,pOut,'=',0);
			ConvEmit(pState,pOut,zHex[(c>>4)&0x0F],0);
			ConvEmit(pState,pOut,zHex[c&0x0F],0);
		}
	}
}
static int ConvHexValue(int c)
{
	if( c >= '0' && c <= '9' ){
		return c - '0';
	}
	if( c >= 'A' && c <= 'F' ){
		return c - 'A' + 10;
	}
	if( c >= 'a' && c <= 'f' ){
		return c - 'a' + 10;
	}
	return -1;
}
/*
 * The decoder carries an unfinished escape: `=`, `=A`, and the `=` of a SOFT
 * line break whose ending has not arrived yet are all states a chunk boundary
 * can land in. A `=` followed by anything that is not two hex digits or a line
 * ending is what php calls an invalid byte sequence.
 */
static void ConvQpDecodeBytes(phl_conv_state *pState,phl_stream_filter *pFilter,
	const unsigned char *zIn,sxu32 nIn,SyBlob *pOut)
{
	sxu32 i;
	for( i = 0 ; i < nIn ; i++ ){
		int c = zIn[i];
		sxu32 nCarry = SyBlobLength(&pFilter->sCarry);
		if( nCarry == 0 ){
			if( c == '=' ){
				char eq = '=';
				SyBlobAppend(&pFilter->sCarry,&eq,1);
			}else{
				char ch = (char)c;
				SyBlobAppend(pOut,&ch,1);
			}
			continue;
		}
		if( nCarry == 1 ){
			if( c == '\r' ){
				/* Wait for the LF (or for the next byte, which decides). */
				char ch = (char)c;
				SyBlobAppend(&pFilter->sCarry,&ch,1);
				continue;
			}
			if( c == '\n' ){
				SyBlobReset(&pFilter->sCarry); /* soft break: both bytes vanish */
				continue;
			}
			if( ConvHexValue(c) < 0 ){
				pState->bErr = 1;
				SyBlobReset(&pFilter->sCarry);
				continue;
			}
			{
				char ch = (char)c;
				SyBlobAppend(&pFilter->sCarry,&ch,1);
			}
			continue;
		}
		{
			const unsigned char *z = (const unsigned char *)SyBlobData(&pFilter->sCarry);
			if( z[1] == '\r' ){
				/* `=\r` then anything: the soft break ends, and a byte that is
				 * not the LF belongs to the output. */
				SyBlobReset(&pFilter->sCarry);
				if( c != '\n' ){
					char ch = (char)c;
					SyBlobAppend(pOut,&ch,1);
				}
				continue;
			}
			if( ConvHexValue(c) < 0 ){
				pState->bErr = 1;
				SyBlobReset(&pFilter->sCarry);
				continue;
			}
			{
				char ch = (char)((ConvHexValue(z[1]) << 4) | ConvHexValue(c));
				SyBlobAppend(pOut,&ch,1);
			}
			SyBlobReset(&pFilter->sCarry);
		}
	}
}
static int ConvFilter(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	phl_conv_state *pState = (phl_conv_state *)pFilter->pPriv;
	int bClosing = (iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE) != 0;
	phl_bucket *pBucket;
	SyBlob sOut;
	if( pState == 0 ){
		return PHL_PSFS_ERR_FATAL;
	}
	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);
	while( (pBucket = FilterBucketPop(pIn)) != 0 ){
		const unsigned char *z = (const unsigned char *)SyBlobData(&pBucket->sData);
		sxu32 n = SyBlobLength(&pBucket->sData);
		switch( pState->iKind ){
		case PHL_CONV_B64_ENCODE: ConvB64EncodeBytes(pState,pFilter,z,n,&sOut,0); break;
		case PHL_CONV_B64_DECODE: ConvB64DecodeBytes(pState,pFilter,z,n,&sOut); break;
		case PHL_CONV_QP_ENCODE:  ConvQpEncodeBytes(pState,z,n,&sOut); break;
		default:                  ConvQpDecodeBytes(pState,pFilter,z,n,&sOut); break;
		}
		PH7_FilterBucketFree(pFilter->pVm,pBucket);
	}
	if( bClosing ){
		switch( pState->iKind ){
		case PHL_CONV_B64_ENCODE: ConvB64EncodeBytes(pState,pFilter,0,0,&sOut,1); break;
		case PHL_CONV_B64_DECODE: ConvB64DecodeTail(pFilter,&sOut); break;
		default:
			/* A dangling `=` at the end of a quoted-printable stream is dropped,
			 * and an unfinished base64 group has already been handled above. */
			SyBlobReset(&pFilter->sCarry);
			break;
		}
	}
	if( pState->bErr ){
		char zMsg[96];
		SyBufferFormat(zMsg,sizeof(zMsg),"Stream filter (%.*s): invalid byte sequence",
			(int)SyBlobLength(&pFilter->sName),(const char *)SyBlobData(&pFilter->sName));
		/* php names the READER in this warning — it is raised from inside the
		 * read, not from the call that attached the filter. */
		PH7_VmThrowError(pFilter->pVm,pFilter->pVm->pCalleeName,PH7_CTX_WARNING,zMsg);
		SyBlobRelease(&sOut);
		return PHL_PSFS_ERR_FATAL;
	}
	if( SyBlobLength(&sOut) > 0 ){
		PH7_FilterBucketAppend(pOut,
			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));
	}
	SyBlobRelease(&sOut);
	return PHL_PSFS_PASS_ON;
}
/* --------------------------------------------------------------------------
 * dechunk — HTTP's chunked transfer encoding, taken apart.
 *
 * php is forgiving here on purpose: a body that is not chunked at all comes
 * back unchanged, an extension after the size (`4;name=value`) is ignored, and
 * a truncated body answers what arrived.
 * -------------------------------------------------------------------------- */
#define PHL_DECHUNK_SIZE  0  /* reading the size line */
#define PHL_DECHUNK_DATA  1  /* copying nRemain bytes */
#define PHL_DECHUNK_CRLF  2  /* eating the ending after a chunk */
#define PHL_DECHUNK_DONE  3  /* the zero-size chunk arrived */
#define PHL_DECHUNK_RAW   4  /* not chunked at all: pass everything through */
typedef struct phl_dechunk_state phl_dechunk_state;
struct phl_dechunk_state
{
	int iPhase;
	ph7_int64 nRemain;
};
static int DechunkCreate(phl_stream_filter *pFilter,ph7_value *pParams)
{
	phl_dechunk_state *pState;
	SXUNUSED(pParams);
	pState = (phl_dechunk_state *)SyMemBackendAlloc(&pFilter->pVm->sAllocator,
		sizeof(phl_dechunk_state));
	if( pState == 0 ){
		return -1;
	}
	SyZero(pState,sizeof(phl_dechunk_state));
	pFilter->pPriv = (void *)pState;
	return PH7_OK;
}
static void DechunkClose(phl_stream_filter *pFilter)
{
	if( pFilter->pPriv ){
		SyMemBackendFree(&pFilter->pVm->sAllocator,pFilter->pPriv);
		pFilter->pPriv = 0;
	}
}
/* Read the size line out of the carry. Answers 1 when one was complete. */
static int DechunkTakeSize(phl_stream_filter *pFilter,phl_dechunk_state *pState)
{
	const char *z = (const char *)SyBlobData(&pFilter->sCarry);
	sxu32 n = SyBlobLength(&pFilter->sCarry);
	sxu32 i,nLine;
	ph7_int64 nSize = 0;
	if( n < 1 ){
		return 0;
	}
	if( ConvHexValue((unsigned char)z[0]) < 0 ){
		/* A size line has to START with a hex digit. php decides "this body is
		 * not chunked" on that ONE byte and passes everything from there on
		 * through unchanged — which is why a body with no line ending at all
		 * still comes back whole, and why the trailer of a truncated chunked
		 * body does too. */
		pState->iPhase = PHL_DECHUNK_RAW;
		return 1;
	}
	for( i = 0 ; i < n ; i++ ){
		if( z[i] == '\n' ){
			break;
		}
	}
	if( i >= n ){
		return 0; /* no ending yet */
	}
	nLine = i;
	for( i = 0 ; i < nLine ; i++ ){
		int v = ConvHexValue((unsigned char)z[i]);
		if( v < 0 ){
			break;
		}
		nSize = nSize * 16 + v;
	}
	/* Drop the line, extension and ending included. */
	{
		SyBlob sRest;
		SyBlobInit(&sRest,&pFilter->pVm->sAllocator);
		if( nLine + 1 < n ){
			SyBlobAppend(&sRest,&z[nLine+1],n - (nLine+1));
		}
		SyBlobReset(&pFilter->sCarry);
		if( SyBlobLength(&sRest) > 0 ){
			SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));
		}
		SyBlobRelease(&sRest);
	}
	pState->nRemain = nSize;
	pState->iPhase = nSize > 0 ? PHL_DECHUNK_DATA : PHL_DECHUNK_DONE;
	return 1;
}
static int DechunkFilter(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	phl_dechunk_state *pState = (phl_dechunk_state *)pFilter->pPriv;
	phl_bucket *pBucket;
	SyBlob sOut;
	SXUNUSED(iFlags);
	if( pState == 0 ){
		return PHL_PSFS_ERR_FATAL;
	}
	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);
	while( (pBucket = FilterBucketPop(pIn)) != 0 ){
		if( SyBlobLength(&pBucket->sData) > 0 ){
			SyBlobAppend(&pFilter->sCarry,SyBlobData(&pBucket->sData),
				SyBlobLength(&pBucket->sData));
		}
		PH7_FilterBucketFree(pFilter->pVm,pBucket);
	}
	for(;;){
		sxu32 nHave = SyBlobLength(&pFilter->sCarry);
		if( pState->iPhase == PHL_DECHUNK_DONE ){
			SyBlobReset(&pFilter->sCarry);
			break;
		}
		if( pState->iPhase == PHL_DECHUNK_RAW ){
			if( nHave > 0 ){
				SyBlobAppend(&sOut,SyBlobData(&pFilter->sCarry),nHave);
				SyBlobReset(&pFilter->sCarry);
			}
			break;
		}
		if( nHave < 1 ){
			break;
		}
		if( pState->iPhase == PHL_DECHUNK_SIZE ){
			if( !DechunkTakeSize(pFilter,pState) ){
				break;
			}
			continue;
		}
		if( pState->iPhase == PHL_DECHUNK_DATA ){
			ph7_int64 nTake = (ph7_int64)nHave;
			if( nTake > pState->nRemain ){
				nTake = pState->nRemain;
			}
			SyBlobAppend(&sOut,SyBlobData(&pFilter->sCarry),(sxu32)nTake);
			{
				SyBlob sRest;
				SyBlobInit(&sRest,&pFilter->pVm->sAllocator);
				if( (sxu32)nTake < nHave ){
					SyBlobAppend(&sRest,
						(const char *)SyBlobData(&pFilter->sCarry) + nTake,nHave - (sxu32)nTake);
				}
				SyBlobReset(&pFilter->sCarry);
				if( SyBlobLength(&sRest) > 0 ){
					SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));
				}
				SyBlobRelease(&sRest);
			}
			pState->nRemain -= nTake;
			if( pState->nRemain < 1 ){
				pState->iPhase = PHL_DECHUNK_CRLF;
			}
			continue;
		}
		/* PHL_DECHUNK_CRLF: the ending that follows a chunk's bytes has to be
		 * RIGHT THERE. php does not go looking for it — anything else means the
		 * body was never chunked to begin with, and the rest passes through. */
		{
			const char *z = (const char *)SyBlobData(&pFilter->sCarry);
			sxu32 i;
			if( z[0] == '\r' ){
				if( nHave < 2 ){
					break; /* the LF may still be coming */
				}
				if( z[1] != '\n' ){
					pState->iPhase = PHL_DECHUNK_RAW;
					continue;
				}
				i = 1;
			}else if( z[0] == '\n' ){
				i = 0;
			}else{
				pState->iPhase = PHL_DECHUNK_RAW;
				continue;
			}
			{
				SyBlob sRest;
				SyBlobInit(&sRest,&pFilter->pVm->sAllocator);
				if( i + 1 < nHave ){
					SyBlobAppend(&sRest,&z[i+1],nHave - (i+1));
				}
				SyBlobReset(&pFilter->sCarry);
				if( SyBlobLength(&sRest) > 0 ){
					SyBlobAppend(&pFilter->sCarry,SyBlobData(&sRest),SyBlobLength(&sRest));
				}
				SyBlobRelease(&sRest);
			}
			pState->iPhase = PHL_DECHUNK_SIZE;
			continue;
		}
	}
	if( SyBlobLength(&sOut) > 0 ){
		PH7_FilterBucketAppend(pOut,
			PH7_FilterBucketNew(pFilter->pVm,SyBlobData(&sOut),SyBlobLength(&sOut)));
	}
	SyBlobRelease(&sOut);
	return PHL_PSFS_PASS_ON;
}
/*
 * The registry, in php's own registration order — which is the order
 * stream_get_filters() answers in. A name ending in `.*` is a FACTORY: php
 * registers `convert.*` once and lets it answer for every convert.<something>,
 * which is why the lookup below falls back to progressively shorter wildcards.
 *
 * php's own list carries two more this build has no engine for — `zlib.*` and
 * `convert.iconv.*` — and one it has no explicable behaviour for: `consumed`
 * passes every byte through (a userland filter placed after it receives them
 * all) and yet php answers "" to `fgets()`, to `fread($h,100)` and to
 * `stream_get_contents()` while answering `fread($h,3)` correctly. It exists
 * for php://input's own bookkeeping; a name whose answer depends on WHICH
 * reader asked is not one to reproduce, so it is left out rather than guessed.
 */
static const phl_filter_ops aBuiltinFilters[] = {
	{ "string.rot13",   0, Rot13Filter,   0 },
	{ "string.toupper", 0, ToUpperFilter, 0 },
	{ "string.tolower", 0, ToLowerFilter, 0 },
	{ "convert.*",      ConvFilterCreate, ConvFilter, ConvFilterClose },
	{ "dechunk",        DechunkCreate,    DechunkFilter, DechunkClose },
};
/*
 * Locate the ops behind a filter NAME. php tries the exact name first, then
 * replaces everything after each trailing `.` with `*` and tries again, so
 * `convert.iconv.utf-8/utf-16` finds `convert.iconv.*` and then `convert.*`.
 * The comparison is case SENSITIVE: php answers `Unable to locate filter` for
 * `STRING.ROT13`.
 */
static const phl_filter_ops * FilterFindOps(const char *zName,int nName)
{
	char zWild[128];
	sxu32 n;
	int nTry;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){
		const char *zCur = aBuiltinFilters[n].zName;
		if( (int)SyStrlen(zCur) == nName && SyMemcmp(zCur,zName,(sxu32)nName) == 0 ){
			return &aBuiltinFilters[n];
		}
	}
	nTry = nName;
	for(;;){
		/* Strip back to (and including) the last period still inside the prefix. */
		while( nTry > 0 && zName[nTry-1] != '.' ){
			nTry--;
		}
		if( nTry < 1 ){
			break;
		}
		if( nTry + 1 < (int)sizeof(zWild) ){
			SyMemcpy(zName,zWild,(sxu32)nTry);
			zWild[nTry] = '*';
			for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){
				const char *zCur = aBuiltinFilters[n].zName;
				if( (int)SyStrlen(zCur) == nTry + 1 && SyMemcmp(zCur,zWild,(sxu32)(nTry+1)) == 0 ){
					return &aBuiltinFilters[n];
				}
			}
		}
		nTry--; /* step past the period we just matched on */
	}
	return 0;
}
/* --------------------------------------------------------------------------
 * Filter instances.
 * -------------------------------------------------------------------------- */
PH7_PRIVATE phl_stream_filter * PH7_StreamFilterFromValue(ph7_value *pVal)
{
	phl_stream_filter *pFilter;
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	pFilter = (phl_stream_filter *)ph7_value_to_resource(pVal);
	if( pFilter == 0 || pFilter->base.iMagic != STREAM_FILTER_MAGIC ){
		return 0;
	}
	return pFilter;
}
/* Allocate one filter, chained on the VM registry so it goes back at reset. */
static phl_stream_filter * FilterNew(ph7_vm *pVm,const phl_filter_ops *pOps,
	const char *zName,int nName)
{
	phl_stream_filter *pFilter;
	pFilter = (phl_stream_filter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_filter));
	if( pFilter == 0 ){
		return 0;
	}
	SyZero(pFilter,sizeof(phl_stream_filter));
	pFilter->base.iMagic = STREAM_FILTER_MAGIC;
	pFilter->pVm = pVm;
	pFilter->pOps = pOps;
	SyBlobInit(&pFilter->sName,&pVm->sAllocator);
	SyBlobInit(&pFilter->sCarry,&pVm->sAllocator);
	if( nName > 0 ){
		SyBlobAppend(&pFilter->sName,zName,(sxu32)nName);
	}
	pFilter->pRegNext = (phl_stream_filter *)pVm->pStreamFilter;
	pVm->pStreamFilter = (void *)pFilter;
	return pFilter;
}
/*
 * Release one filter's own resources. The instance itself stays allocated until
 * the VM resets — a ph7_value the script still holds names this pointer, and a
 * probe of it has to stay in bounds — so the magic becomes the CLOSED one,
 * which is what makes `is_resource($f)` false after stream_filter_remove()
 * exactly as php reports it.
 */
static void FilterDispose(phl_stream_filter *pFilter)
{
	if( pFilter->pOps && pFilter->pOps->xClose ){
		pFilter->pOps->xClose(pFilter);
	}
	SyBlobRelease(&pFilter->sCarry);
	pFilter->pDev = 0;
	pFilter->pNext = 0;
	pFilter->base.iMagic = IO_PRIVATE_CLOSED_MAGIC;
}
/* --------------------------------------------------------------------------
 * Running a chain.
 * -------------------------------------------------------------------------- */
/* One filter's turn. Built-in ops run their routine; the userland half hooks in
 * here when it lands. */
static int FilterInvoke(phl_stream_filter *pFilter,phl_brigade *pIn,phl_brigade *pOut,int iFlags)
{
	if( pFilter->pOps == 0 || pFilter->pOps->xFilter == 0 ){
		return PHL_PSFS_ERR_FATAL;
	}
	if( iFlags & PHL_PSFS_FLAG_FLUSH_CLOSE ){
		if( pFilter->bClosed ){
			/* A filter gets exactly ONE closing call: the device's end already
			 * made it, and running a buffering codec's tail a second time (a
			 * stream_filter_remove() after the last read, say) would emit that
			 * tail twice. Whatever arrives now simply passes through. */
			phl_bucket *pBucket;
			while( (pBucket = FilterBucketPop(pIn)) != 0 ){
				PH7_FilterBucketAppend(pOut,pBucket);
			}
			return PHL_PSFS_PASS_ON;
		}
		pFilter->bClosed = 1;
	}
	{
		int rc = pFilter->pOps->xFilter(pFilter,pIn,pOut,iFlags);
		if( rc == PHL_PSFS_ERR_FATAL ){
			/* Marked, not skipped: php runs a filter that has already refused
			 * once again on the next write and reports the refusal again — what
			 * it does NOT do is run it a last time at close. */
			pFilter->bDead = 1;
		}
		return rc;
	}
}
/*
 * iFlags describes the call for the HEAD of the chain and iRestFlags for
 * everything behind it, because the two are not always the same: the device's
 * end of file closes every filter on the stream, but flushing ONE filter — what
 * stream_filter_remove() does — closes only that one and hands its tail to the
 * others as ordinary data. Closing them too would make a codec below emit its
 * own tail early: removing an upstream `string.toupper` from a chain ending in
 * `convert.base64-encode` padded the base64 there and then, where php leaves it
 * mid-group.
 */
PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,
	const void *pData,sxu32 nLen,int iFlags,int iRestFlags,SyBlob *pOut)
{
	ph7_vm *pVm = pHead->pVm;
	phl_brigade sA,sB;
	phl_brigade *pIn,*pOutBrig,*pSwap;
	phl_stream_filter *pFilter;
	phl_bucket *pBucket;
	int iStatus = PHL_PSFS_PASS_ON;
	SyZero(&sA,sizeof(sA));
	SyZero(&sB,sizeof(sB));
	if( nLen > 0 ){
		pBucket = PH7_FilterBucketNew(pVm,pData,nLen);
		if( pBucket == 0 ){
			return PHL_PSFS_ERR_FATAL;
		}
		PH7_FilterBucketAppend(&sA,pBucket);
	}
	pIn = &sA;
	pOutBrig = &sB;
	for( pFilter = pHead ; pFilter ; pFilter = pFilter->pNext ){
		iStatus = FilterInvoke(pFilter,pIn,pOutBrig,pFilter == pHead ? iFlags : iRestFlags);
		if( iStatus != PHL_PSFS_PASS_ON ){
			break;
		}
		/* Whatever the filter left behind is dropped: php warns about it from
		 * the reader ("Unprocessed filter buckets remaining on input brigade")
		 * and hands the read back as a failure, which is the ERR_FATAL path. */
		PH7_FilterBrigadeRelease(pVm,pIn);
		/* This filter's output is the next one's input. */
		pSwap = pIn;
		pIn = pOutBrig;
		pOutBrig = pSwap;
	}
	if( iStatus == PHL_PSFS_PASS_ON && pOut ){
		while( (pBucket = FilterBucketPop(pIn)) != 0 ){
			if( SyBlobLength(&pBucket->sData) > 0 ){
				SyBlobAppend(pOut,SyBlobData(&pBucket->sData),SyBlobLength(&pBucket->sData));
			}
			PH7_FilterBucketFree(pVm,pBucket);
		}
	}
	PH7_FilterBrigadeRelease(pVm,&sA);
	PH7_FilterBrigadeRelease(pVm,&sB);
	return iStatus;
}
/* --------------------------------------------------------------------------
 * Attaching, removing and releasing.
 * -------------------------------------------------------------------------- */
/* The chain head slot of a handle for one direction. */
static phl_stream_filter ** FilterChainSlot(io_private *pDev,int iChain)
{
	if( iChain == PHL_STREAM_FILTER_WRITE ){
		return (phl_stream_filter **)&pDev->pWriteFilters;
	}
	return (phl_stream_filter **)&pDev->pReadFilters;
}
/* Unlink a filter from the chain it sits on. */
static void FilterUnlink(phl_stream_filter *pFilter)
{
	phl_stream_filter **ppSlot,*pCur;
	if( pFilter->pDev == 0 ){
		return;
	}
	ppSlot = FilterChainSlot(pFilter->pDev,pFilter->iChain);
	pCur = *ppSlot;
	if( pCur == pFilter ){
		*ppSlot = pFilter->pNext;
		return;
	}
	while( pCur ){
		if( pCur->pNext == pFilter ){
			pCur->pNext = pFilter->pNext;
			return;
		}
		pCur = pCur->pNext;
	}
}
/*
 * The last call a filter ever gets. A write filter's tail has to reach the
 * device, and a read filter's has to reach the reader, so a flush is a chain
 * run from THIS filter down with no input and the closing flag.
 */
static void FilterFlushTail(phl_stream_filter *pFilter,int iRestFlags)
{
	io_private *pDev = pFilter->pDev;
	phl_stream_filter *pCur;
	SyBlob sOut;
	if( pDev == 0 ){
		return;
	}
	for( pCur = pFilter ; pCur ; pCur = pCur->pNext ){
		if( pCur->bDead ){
			/* A chain that already refused its input is finished: php does not
			 * run it again at close, and running it here would report the same
			 * refusal a second time from fclose(). */
			return;
		}
	}
	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);
	if( PH7_FilterChainProcess(pFilter,0,0,PHL_PSFS_FLAG_FLUSH_CLOSE,iRestFlags,&sOut)
	    == PHL_PSFS_PASS_ON && SyBlobLength(&sOut) > 0 ){
		if( pFilter->iChain == PHL_STREAM_FILTER_WRITE ){
			if( pDev->pStream && pDev->pStream->xWrite ){
				pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),
					(ph7_int64)SyBlobLength(&sOut));
			}
		}else{
			SyBlobAppend(&pDev->sFilt,SyBlobData(&sOut),SyBlobLength(&sOut));
		}
	}
	SyBlobRelease(&sOut);
}
PH7_PRIVATE void PH7_StreamFilterReleaseChains(io_private *pDev)
{
	int i;
	for( i = 0 ; i < 2 ; i++ ){
		int iChain = i == 0 ? PHL_STREAM_FILTER_WRITE : PHL_STREAM_FILTER_READ;
		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);
		phl_stream_filter *pFilter = *ppSlot;
		/* The WRITE chain is flushed first and as a whole: the head's tail has
		 * to travel through the filters below it before anything reaches the
		 * device. */
		if( iChain == PHL_STREAM_FILTER_WRITE && pFilter ){
			FilterFlushTail(pFilter,PHL_PSFS_FLAG_FLUSH_CLOSE);
		}
		while( pFilter ){
			phl_stream_filter *pNext = pFilter->pNext;
			FilterDispose(pFilter);
			pFilter = pNext;
		}
		*ppSlot = 0;
	}
}
PH7_PRIVATE void PH7_StreamFilterRewound(io_private *pDev)
{
	int i;
	for( i = 0 ; i < 2 ; i++ ){
		phl_stream_filter *pFilter = *FilterChainSlot(pDev,
			i == 0 ? PHL_STREAM_FILTER_READ : PHL_STREAM_FILTER_WRITE);
		while( pFilter ){
			/* The stream moved, so the end it had reached is not the end any
			 * more: a chain closed at the old one must be able to run — and to
			 * emit its tail — again. */
			pFilter->bClosed = 0;
			pFilter = pFilter->pNext;
		}
	}
}
PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)
{
	phl_stream_filter *pFilter;
	if( pVm == 0 ){
		return;
	}
	pFilter = (phl_stream_filter *)pVm->pStreamFilter;
	while( pFilter ){
		phl_stream_filter *pNext = pFilter->pRegNext;
		if( pFilter->base.iMagic == STREAM_FILTER_MAGIC ){
			/* The std handles outlive a reset (the -S server reuses one VM), so
			 * a filter that was never removed has to leave their chain before
			 * its memory goes back — otherwise the next request's first write
			 * walks a freed one. */
			io_private *pDev = pFilter->pDev;
			FilterUnlink(pFilter);
			if( pDev ){
				SyBlobReset(&pDev->sFilt);
				pDev->nFiltOfft = 0;
				pDev->bFiltDone = 0;
			}
			FilterDispose(pFilter);
		}
		SyBlobRelease(&pFilter->sName);
		pFilter->base.iMagic = 0;
		SyMemBackendFree(&pVm->sAllocator,pFilter);
		pFilter = pNext;
	}
	pVm->pStreamFilter = 0;
}
PH7_PRIVATE phl_stream_filter * PH7_StreamFilterAttach(ph7_context *pCtx,io_private *pDev,
	const char *zName,int nName,int iChain,int bPrepend,ph7_value *pParams)
{
	const phl_filter_ops *pOps;
	phl_stream_filter *pFilter;
	pOps = FilterFindOps(zName,nName);
	if( pOps == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to locate filter \"%.*s\"",nName,zName);
		return 0;
	}
	pFilter = FilterNew(pCtx->pVm,pOps,zName,nName);
	if( pFilter == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		return 0;
	}
	if( pOps->xCreate && pOps->xCreate(pFilter,pParams) != PH7_OK ){
		FilterDispose(pFilter);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to create or locate filter \"%.*s\"",nName,zName);
		return 0;
	}
	pFilter->pDev = pDev;
	pFilter->iChain = iChain;
	if( bPrepend ){
		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);
		pFilter->pNext = *ppSlot;
		*ppSlot = pFilter;
	}else{
		phl_stream_filter **ppSlot = FilterChainSlot(pDev,iChain);
		phl_stream_filter *pCur = *ppSlot;
		if( pCur == 0 ){
			*ppSlot = pFilter;
		}else{
			while( pCur->pNext ){
				pCur = pCur->pNext;
			}
			pCur->pNext = pFilter;
		}
	}
	return pFilter;
}
/* --------------------------------------------------------------------------
 * The builtins.
 * -------------------------------------------------------------------------- */
/*
 * resource|false stream_filter_append(resource $stream, string $filter_name,
 *                                     int $mode = 0, mixed $params = null)
 * resource|false stream_filter_prepend(...)
 *
 * php's $mode of 0 is not "no chain": it means "whichever chains this handle's
 * MODE makes sense for", so a stream opened `r+` gets the filter on BOTH — two
 * separate instances, since a filter carries state and one cannot serve two
 * directions. The resource answered is the LAST one created, which is why
 * removing what `stream_filter_append($h,'…')` gave back on an `r+` handle
 * leaves the READ half of it still filtering.
 */
static int StreamFilterAddCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPrepend)
{
	phl_stream_filter *pFilter = 0;
	ph7_value *pParams;
	io_private *pDev;
	const char *zName;
	int nName,iChain,rc;
	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);
	if( pDev == 0 ){
		return rc;
	}
	zName = ph7_value_to_string(apArg[1],&nName);
	iChain = nArg > 2 ? (int)ph7_value_to_int(apArg[2]) : 0;
	pParams = nArg > 3 ? apArg[3] : 0;
	if( iChain == 0 ){
		/* php reads the mode the handle was OPENED with. */
		const char *zMode = pDev->zMode;
		sxu32 nDummy;
		int bPlus = SyByteFind(zMode,SyStrlen(zMode),'+',&nDummy) == SXRET_OK;
		switch( zMode[0] ){
		case 'r':
			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_READ;
			break;
		case 'w':
		case 'a':
		case 'x':
		case 'c':
			iChain = bPlus ? PHL_STREAM_FILTER_ALL : PHL_STREAM_FILTER_WRITE;
			break;
		default:
			break;
		}
	}
	if( iChain & PHL_STREAM_FILTER_READ ){
		pFilter = PH7_StreamFilterAttach(pCtx,pDev,zName,nName,PHL_STREAM_FILTER_READ,
			bPrepend,pParams);
		if( pFilter == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	if( iChain & PHL_STREAM_FILTER_WRITE ){
		pFilter = PH7_StreamFilterAttach(pCtx,pDev,zName,nName,PHL_STREAM_FILTER_WRITE,
			bPrepend,pParams);
		if( pFilter == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	if( pFilter == 0 ){
		/* A mode this engine could not place the filter on. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_resource(pCtx,pFilter);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_filter_append(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StreamFilterAddCommon(pCtx,nArg,apArg,0);
}
PH7_PRIVATE int PH7_builtin_stream_filter_prepend(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StreamFilterAddCommon(pCtx,nArg,apArg,1);
}
/*
 * bool stream_filter_remove(resource $stream_filter)
 *
 * php FLUSHES the filter on the way out — a write filter's tail still reaches
 * the device and a read filter's still reaches the reader — and then the
 * resource is dead: passing it again is a TypeError, not FALSE.
 */
PH7_PRIVATE int PH7_builtin_stream_filter_remove(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_filter *pFilter;
	SXUNUSED(nArg);
	if( !ph7_value_is_resource(apArg[0]) ){
		/* php's ZPP runs first: a string is not "the wrong resource", it is not
		 * a resource at all, and the two diagnostics are different. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 ($stream_filter) must be of type resource, %s given",
			ph7_function_name(pCtx),ph7_type_name(apArg[0]));
	}
	pFilter = PH7_StreamFilterFromValue(apArg[0]);
	if( pFilter == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): supplied resource is not a valid stream filter resource",
			ph7_function_name(pCtx));
	}
	/* Only THIS filter closes; what it emits travels through the rest of the
	 * chain as ordinary data, because those filters stay on the stream. */
	FilterFlushTail(pFilter,PHL_PSFS_FLAG_NORMAL);
	FilterUnlink(pFilter);
	FilterDispose(pFilter);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * array stream_get_filters(void)
 *  The filter names this build can create, in php's own registration order.
 */
PH7_PRIVATE int PH7_builtin_stream_get_filters(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltinFilters) ; ++n ){
		ph7_value_string(pValue,aBuiltinFilters[n].zName,-1);
		ph7_array_add_elem(pArray,0,pValue);
		ph7_value_reset_string_cursor(pValue);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
#endif /* PH7_DISABLE_DISK_IO */
