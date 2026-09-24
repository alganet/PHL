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
/*
 * The registry, in php's own registration order — which is the order
 * stream_get_filters() answers in. A name ending in `.*` is a FACTORY: php
 * registers `convert.*` once and lets it answer for every convert.<something>,
 * which is why the lookup below falls back to progressively shorter wildcards.
 */
static const phl_filter_ops aBuiltinFilters[] = {
	{ "string.rot13",   0, Rot13Filter,   0 },
	{ "string.toupper", 0, ToUpperFilter, 0 },
	{ "string.tolower", 0, ToLowerFilter, 0 },
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
	return pFilter->pOps->xFilter(pFilter,pIn,pOut,iFlags);
}
PH7_PRIVATE int PH7_FilterChainProcess(phl_stream_filter *pHead,
	const void *pData,sxu32 nLen,int iFlags,SyBlob *pOut)
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
		iStatus = FilterInvoke(pFilter,pIn,pOutBrig,iFlags);
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
static void FilterFlushTail(phl_stream_filter *pFilter)
{
	io_private *pDev = pFilter->pDev;
	SyBlob sOut;
	if( pDev == 0 ){
		return;
	}
	SyBlobInit(&sOut,&pFilter->pVm->sAllocator);
	if( PH7_FilterChainProcess(pFilter,0,0,PHL_PSFS_FLAG_FLUSH_CLOSE,&sOut)
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
			FilterFlushTail(pFilter);
		}
		while( pFilter ){
			phl_stream_filter *pNext = pFilter->pNext;
			FilterDispose(pFilter);
			pFilter = pNext;
		}
		*ppSlot = 0;
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
	FilterFlushTail(pFilter);
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
