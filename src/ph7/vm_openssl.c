/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_OPENSSL
#include "openssl_int.h"

/*
 * Section:
 *    ext/openssl -- php's binding of OpenSSL. This unit carries the
 *    library-wide surface (the version, the error ring, the algorithm
 *    listings), the DIGEST and CIPHER halves, and the random/derivation
 *    helpers. The certificate containers live in vm_openssl_x509.c.
 * Status:
 *    In progress.
 *
 * WHY A BINDING AND NOT A REIMPLEMENTATION. Same call as ext/zlib and
 * ext/curl, and for the sharpest version of the reason those two record: the
 * bytes a program depends on here are a SPECIFIC library's. AES-256-GCM has
 * one answer, but which cipher NAMES exist, what an unknown one says, which
 * of them are AEAD, what a decode failure's error string reads and what
 * OPENSSL_VERSION_TEXT is are all the linked OpenSSL's, and a program that
 * encrypts under php and decrypts under PHL needs exactly them. So every
 * answer below is derived by asking php 8.5 and OpenSSL 3.0 the same
 * question, never by reading php's source -- see the per-function notes for
 * the ones a careful reading would have got wrong.
 *
 * TWO CRYPTO STACKS. This is not the TLS ext/curl uses: libcurl carries its
 * own backend (OpenSSL on Linux, Schannel through vcpkg on Windows), so on
 * Windows the binary holds two implementations that share nothing. php's own
 * Windows build has exactly the same split, which is why `curl_version()`'s
 * ssl_version and OPENSSL_VERSION_TEXT are two different questions there and
 * Composer's platform repository asks both.
 *
 * MEMORY MODEL. OpenSSL stays on its own (system) allocator, like libcurl,
 * libxml2 and sqlite3 before it: routing it through SyMemBackend would subject
 * library internals to the PHL_MAX_ALLOC fault injection the stress tier uses,
 * and a half-initialized EVP context is not something OpenSSL unwinds.
 *
 * WHAT IS VERSION-DEPENDENT, AND THEREFORE NOT PINNED. The algorithm listings
 * (`openssl_get_md_methods`, `openssl_get_cipher_methods`,
 * `openssl_get_curve_names`), OPENSSL_VERSION_TEXT/NUMBER and the error
 * strings all move with the linked library -- this box's 3.0.13 answers 21 and
 * 124 where the Windows guest's 3.6.3 answers something else. The corpus
 * therefore pins round trips, refusals and the shape of a listing, never its
 * length. Same discipline the `libxml-version-answers-differ` note records for
 * ext/dom.
 */

/* ------------------------------------------------------------------------
 * php's error ring
 * ------------------------------------------------------------------------ */
/*
 * Drain OpenSSL's thread error queue into the VM's ring. php does this after
 * an operation fails rather than handing the queue over, and the ring is what
 * openssl_error_string() reads. A full ring drops its OLDEST entry, which is
 * why twenty-five failures answer the last fifteen.
 */
PH7_PRIVATE void PH7_SslStoreErrors(ph7_vm *pVm)
{
	phl_ssl_errors *pRing = (phl_ssl_errors *)pVm->pSslErrors;
	unsigned long iErr;
	if( pRing == 0 ){
		pRing = (phl_ssl_errors *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ssl_errors));
		if( pRing == 0 ){
			ERR_clear_error();
			return;
		}
		SyZero(pRing,sizeof(phl_ssl_errors));
		pVm->pSslErrors = pRing;
	}
	while( (iErr = ERR_get_error()) != 0 ){
		pRing->iTop = (pRing->iTop + 1) % PHL_SSL_ERR_RING;
		pRing->aErr[pRing->iTop] = (int)iErr;
		if( pRing->iTop == pRing->iBottom ){
			pRing->iBottom = (pRing->iBottom + 1) % PHL_SSL_ERR_RING;
		}
	}
}
/*
 * openssl_error_string(): one entry per call, oldest first, false when the
 * ring is empty. The drain happens HERE too, not only on the failure paths --
 * `openssl_digest($s,'nope')` is a refusal php screens by name and never
 * stores for, and its `error:0308010C:...` still comes back.
 */
static int vm_builtin_openssl_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_ssl_errors *pRing;
	unsigned long iErr;
	char zBuf[256];
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_SslStoreErrors(pVm);
	pRing = (phl_ssl_errors *)pVm->pSslErrors;
	if( pRing == 0 || pRing->iTop == pRing->iBottom ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRing->iBottom = (pRing->iBottom + 1) % PHL_SSL_ERR_RING;
	iErr = (unsigned long)(long)pRing->aErr[pRing->iBottom];
	if( pRing->aErr[pRing->iBottom] == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ERR_error_string_n(iErr,zBuf,sizeof(zBuf));
	ph7_result_string(pCtx,zBuf,-1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * The handle records
 * ------------------------------------------------------------------------ */
/*
 * The three handle classes keep their library pointer in the same hidden
 * `__res` slot ext/curl's do, and for the same reason: a PH7 resource carries
 * no destructor, so the record is ALSO chained on a per-VM registry and the
 * sweep at reset/release is what frees a handle a script dropped without
 * unsetting.
 */
static void SslBlankSlot(ph7_class_instance *pOwner)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pOwner == 0 ){
		return;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pOwner,&sAttr);
	if( pRes ){
		PH7_MemObjRelease(pRes);
		MemObjSetType(pRes,MEMOBJ_NULL);
	}
}
static phl_ssl_obj * SslRecOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || !ph7_value_is_resource(pRes) ){
		return 0;
	}
	return (phl_ssl_obj *)pRes->x.pOther;
}
static int SslAttachSlot(ph7_class_instance *pThis,phl_ssl_obj *pRec)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	pRes->x.pOther = (void *)pRec;
	MemObjSetType(pRes,MEMOBJ_RES);
	return 0;
}
/* Free the library object a record owns. The record itself lives in the VM's
 * allocator and is released with it. */
PH7_PRIVATE void PH7_SslFreeObject(phl_ssl_obj *pObj)
{
	if( pObj == 0 || pObj->pHandle == 0 ){
		return;
	}
	switch( pObj->iKind ){
		case PHL_SSL_KIND_CERT: X509_free((X509 *)pObj->pHandle);         break;
		case PHL_SSL_KIND_CSR:  X509_REQ_free((X509_REQ *)pObj->pHandle); break;
		case PHL_SSL_KIND_KEY:  EVP_PKEY_free((EVP_PKEY *)pObj->pHandle); break;
		default: break;
	}
	pObj->pHandle = 0;
}
/*
 * The object is going away: free its library handle now rather than at VM
 * reset, so a script that drops its last reference releases the key there.
 * The shell stays on the registry (the sweep frees it) because the slot is
 * still reachable while the instance is being torn down.
 */
static void SslInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_ssl_obj *pObj = SslRecOfInstance(pThis);
	SXUNUSED(pVm);
	if( pObj == 0 || pObj->pOwner != pThis ){
		return;
	}
	PH7_SslFreeObject(pObj);
	pObj->pOwner = 0;
}
/*
 * Build one handle object: the instance, its record and the registry link.
 * Answers the record; *ppInst is the instance the caller hands back.
 */
PH7_PRIVATE phl_ssl_obj * PH7_SslNewObject(ph7_vm *pVm,int iKind,void *pHandle,
	ph7_class_instance **ppInst)
{
	static const char * const azClass[] = {
		PHL_SSL_CLASS_CERT, PHL_SSL_CLASS_CSR, PHL_SSL_CLASS_KEY
	};
	ph7_class_instance *pInst;
	ph7_class *pClass;
	phl_ssl_obj *pObj;
	const char *zClass;
	if( iKind < 0 || iKind > PHL_SSL_KIND_KEY ){
		return 0;
	}
	zClass = azClass[iKind];
	pClass = PH7_VmExtractClass(pVm,zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	if( pClass == 0 ){
		return 0;
	}
	pInst = PH7_NewClassInstance(pVm,pClass);
	if( pInst == 0 ){
		return 0;
	}
	pObj = (phl_ssl_obj *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_ssl_obj));
	if( pObj == 0 ){
		return 0;
	}
	SyZero(pObj,sizeof(phl_ssl_obj));
	pObj->iKind = iKind;
	pObj->pHandle = pHandle;
	pObj->pOwner = pInst;
	pObj->pNext = (phl_ssl_obj *)pVm->pSslObjs;
	pVm->pSslObjs = pObj;
	if( SslAttachSlot(pInst,pObj) != 0 ){
		SslBlankSlot(pInst);
		pObj->pOwner = 0;
		pObj->pHandle = 0;
		return 0;
	}
	if( ppInst ){
		*ppInst = pInst;
	}
	return pObj;
}
/* The library pointer behind a value that IS one of the three handle classes,
 * or 0 for anything else -- including a handle whose owner has been closed. */
PH7_PRIVATE void * PH7_SslHandleOf(ph7_value *pVal,int iKind)
{
	phl_ssl_obj *pObj;
	if( pVal == 0 || !ph7_value_is_object(pVal) ){
		return 0;
	}
	pObj = SslRecOfInstance((ph7_class_instance *)pVal->x.pOther);
	if( pObj == 0 || pObj->iKind != iKind ){
		return 0;
	}
	return pObj->pHandle;
}
/* Hand a freshly created library object back as this call's return value. The
 * handle is CONSUMED: on failure it is freed here rather than leaked. */
PH7_PRIVATE int PH7_SslResultObject(ph7_context *pCtx,int iKind,void *pHandle)
{
	ph7_class_instance *pInst = 0;
	phl_ssl_obj *pObj;
	if( pHandle == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pObj = PH7_SslNewObject(pCtx->pVm,iKind,pHandle,&pInst);
	if( pObj == 0 || pInst == 0 ){
		switch( iKind ){
			case PHL_SSL_KIND_CERT: X509_free((X509 *)pHandle);         break;
			case PHL_SSL_KIND_CSR:  X509_REQ_free((X509_REQ *)pHandle); break;
			case PHL_SSL_KIND_KEY:  EVP_PKEY_free((EVP_PKEY *)pHandle); break;
			default: break;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pInst);
	return PH7_OK;
}
/* Free every registered handle. Called from the VM reset (a reused VM must not
 * inherit the previous program's keys) and from the release before the
 * allocator holding the records goes away. */
static void SslFreeAllObjects(ph7_vm *pVm)
{
	phl_ssl_obj *pObj = (phl_ssl_obj *)pVm->pSslObjs;
	while( pObj ){
		phl_ssl_obj *pNext = pObj->pNext;
		PH7_SslFreeObject(pObj);
		SyMemBackendFree(&pVm->sAllocator,pObj);
		pObj = pNext;
	}
	pVm->pSslObjs = 0;
}
PH7_PRIVATE void PH7_SslVmReset(ph7_vm *pVm)
{
	SslFreeAllObjects(pVm);
	if( pVm->pSslErrors ){
		SyZero(pVm->pSslErrors,sizeof(phl_ssl_errors));
	}
	ERR_clear_error();
}
PH7_PRIVATE void PH7_SslVmRelease(ph7_vm *pVm)
{
	SslFreeAllObjects(pVm);
	pVm->pSslErrors = 0;   /* the allocator releases the ring with everything else */
	ERR_clear_error();
}

/* ------------------------------------------------------------------------
 * Algorithm lookup
 * ------------------------------------------------------------------------ */
/*
 * The EVP_MD behind php's `string|int $algorithm`. An INT is one of php's own
 * OPENSSL_ALGO_* -- a numbering of php's, not OpenSSL's, which is why the
 * table is spelled here rather than derived. A STRING goes through TWO doors
 * in a fixed order, and the order is observable rather than cosmetic:
 * EVP_get_digestbyname first (which knows the aliases, `RSA-SHA256` among
 * them, and pushes NOTHING when it misses), then EVP_MD_fetch (which knows
 * only the provider's canonical spellings and DOES push
 * `error:0308010C:digital envelope routines::unsupported` on a miss). Fetching
 * first would leave that error in the ring after every successful alias
 * lookup, and openssl_error_string() would hand a script an error for a call
 * that worked.
 *
 * A fetched digest is reference-counted and has to be freed; a legacy one must
 * not be. *ppFetched is the one the caller owns.
 */
PH7_PRIVATE const EVP_MD * PH7_SslDigestOfValue(ph7_value *pVal,EVP_MD **ppFetched)
{
	const EVP_MD *pMd = 0;
	*ppFetched = 0;
	if( pVal == 0 ){
		return 0;
	}
	if( ph7_value_is_string(pVal) ){
		const char *zName;
		int nName = 0;
		zName = ph7_value_to_string(pVal,&nName);
		if( nName < 1 ){
			return 0;
		}
		pMd = EVP_get_digestbyname(zName);
		if( pMd == 0 ){
			*ppFetched = EVP_MD_fetch(0,zName,0);
			pMd = *ppFetched;
		}
		return pMd;
	}
	/* php's OPENSSL_ALGO_* numbering. The gap at 4/5 is php's own -- the two
	 * DSS entries it removed -- and a number outside the set is unknown
	 * rather than a fallback. */
	switch( (int)ph7_value_to_int64(pVal) ){
		case 1:  pMd = EVP_sha1();      break;
		case 2:  pMd = EVP_md5();       break;
		case 3:  pMd = EVP_md4();       break;
		case 6:  pMd = EVP_sha224();    break;
		case 7:  pMd = EVP_sha256();    break;
		case 8:  pMd = EVP_sha384();    break;
		case 9:  pMd = EVP_sha512();    break;
		case 10: pMd = EVP_ripemd160(); break;
		default: pMd = 0;               break;
	}
	return pMd;
}
/*
 * The EVP_CIPHER behind a name. Two doors, because OpenSSL 3 has two: the
 * legacy OBJ table (EVP_get_cipherbyname, which knows "aes-256-cbc" and every
 * alias) and the provider fetch (which knows the canonical names a
 * openssl_get_cipher_methods() listing is built from). php's "Unknown cipher
 * algorithm" is what BOTH missing looks like. A fetched cipher is
 * reference-counted and has to be freed; a legacy one must not be.
 */
static const EVP_CIPHER * SslCipherByName(const char *zName,EVP_CIPHER **ppFetched)
{
	const EVP_CIPHER *pCipher;
	*ppFetched = 0;
	pCipher = EVP_get_cipherbyname(zName);
	if( pCipher ){
		return pCipher;
	}
	*ppFetched = EVP_CIPHER_fetch(0,zName,0);
	return *ppFetched;
}

/* ------------------------------------------------------------------------
 * The listings
 * ------------------------------------------------------------------------ */
/*
 * A name collector: the two listings differ in where the names COME FROM, not
 * in what happens to them.
 */
typedef struct SslNameSink SslNameSink;
struct SslNameSink {
	SySet sOfft;       /* sxu32 offsets into sPool -- the pool MOVES as it grows,
	                    * so a pointer collected during the walk would dangle */
	SyBlob sPool;
	int bLower;        /* the cipher listing lowercases every name; the digest one does not */
	int bAliases;      /* an alias listing keeps the rows a plain one drops */
};
static int SslStrCmp(const char *zL,const char *zR)
{
	while( *zL && *zL == *zR ){
		++zL;
		++zR;
	}
	return (int)((unsigned char)*zL) - (int)((unsigned char)*zR);
}
static void SslSinkAdd(SslNameSink *pSink,const char *zName)
{
	sxu32 nOfft;
	int n,i;
	if( zName == 0 ){
		return;
	}
	n = (int)SyStrlen(zName);
	if( n < 1 ){
		return;
	}
	nOfft = SyBlobLength(&pSink->sPool);
	if( SyBlobAppend(&pSink->sPool,zName,(sxu32)n) != SXRET_OK
	 || SyBlobAppend(&pSink->sPool,"",1) != SXRET_OK ){
		return;
	}
	if( pSink->bLower ){
		char *zCopy = (char *)SyBlobData(&pSink->sPool) + nOfft;
		for( i = 0 ; i < n ; ++i ){
			zCopy[i] = (char)SyToLower(zCopy[i]);
		}
	}
	SySetPut(&pSink->sOfft,(const void *)&nOfft);
}
/* Sort (plain byte order, which is what php's listings come back in) and
 * build the array, dropping the duplicates an alias walk produces. */
static void SslSinkFlush(ph7_context *pCtx,SslNameSink *pSink,ph7_value *pArray,ph7_value *pVal)
{
	sxu32 *aOfft = (sxu32 *)SySetBasePtr(&pSink->sOfft);
	sxu32 nUsed = SySetUsed(&pSink->sOfft);
	const char *zBase = (const char *)SyBlobData(&pSink->sPool);
	const char *zPrev = 0;
	sxu32 i,j;
	SXUNUSED(pCtx);
	/* Insertion sort: the lists are a few hundred rows at most and this needs
	 * no context-carrying qsort (whose spelling differs per platform). */
	for( i = 1 ; i < nUsed ; ++i ){
		sxu32 nKey = aOfft[i];
		j = i;
		while( j > 0 && SslStrCmp(zBase + aOfft[j - 1],zBase + nKey) > 0 ){
			aOfft[j] = aOfft[j - 1];
			--j;
		}
		aOfft[j] = nKey;
	}
	for( i = 0 ; i < nUsed ; ++i ){
		const char *zName = zBase + aOfft[i];
		if( zPrev && SslStrCmp(zPrev,zName) == 0 ){
			continue;
		}
		zPrev = zName;
		ph7_value_string(pVal,zName,-1);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
}
/*
 * The DIGEST walk. php reads digests out of the legacy OBJ_NAME table, where
 * an ALIAS is a row that names a target: the plain listing keeps the rows with
 * no target and the alias listing keeps them all.
 */
static void SslMdListCb(const EVP_MD *pMd,const char *zFrom,const char *zTo,void *pArg)
{
	SslNameSink *pSink = (SslNameSink *)pArg;
	SXUNUSED(pMd);
	if( zTo != 0 && pSink->bAliases == 0 ){
		return;
	}
	SslSinkAdd(pSink,zFrom);
}
static void SslCipherNameCb(const char *zName,void *pArg)
{
	SslSinkAdd((SslNameSink *)pArg,zName);
}
static void SslCipherListCb(EVP_CIPHER *pCipher,void *pArg)
{
	SslNameSink *pSink = (SslNameSink *)pArg;
	if( pSink->bAliases ){
		EVP_CIPHER_names_do_all(pCipher,SslCipherNameCb,pArg);
	}else{
		SslSinkAdd(pSink,EVP_CIPHER_get0_name(pCipher));
	}
}
/*
 * openssl_get_md_methods() and openssl_get_cipher_methods() are NOT the same
 * listing with a different table, and finding that out cost a differential
 * sweep: php reads DIGESTS out of the legacy OBJ_NAME table (which is where
 * `blake2b512` comes from -- the provider calls it `BLAKE2B-512`, and php's
 * answer has no such row) and CIPHERS out of the provider, lowercasing every
 * name and dropping the duplicates that produces. Reading either the way the
 * other is read answers a list php never gives: 22 digests instead of 21, or
 * 170 cipher aliases instead of 234.
 */
static int SslMethodList(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCipher)
{
	SslNameSink sSink;
	ph7_value *pArray,*pVal;
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SySetInit(&sSink.sOfft,&pCtx->pVm->sAllocator,sizeof(sxu32));
	SyBlobInit(&sSink.sPool,&pCtx->pVm->sAllocator);
	sSink.bLower = bCipher;
	sSink.bAliases = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	if( bCipher ){
		EVP_CIPHER_do_all_provided(0,SslCipherListCb,(void *)&sSink);
	}else{
		EVP_MD_do_all_sorted(SslMdListCb,(void *)&sSink);
	}
	SslSinkFlush(pCtx,&sSink,pArray,pVal);
	SySetRelease(&sSink.sOfft);
	SyBlobRelease(&sSink.sPool);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
static int vm_builtin_openssl_get_md_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMethodList(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_get_cipher_methods(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMethodList(pCtx,nArg,apArg,1);
}
/*
 * openssl_get_curve_names(): the library's built-in EC curves, by SHORT name,
 * in the order EC_get_builtin_curves answers -- not sorted, which is a
 * difference from the two listings above and is php's.
 */
static int vm_builtin_openssl_get_curve_names(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EC_builtin_curve *aCurve;
	ph7_value *pArray,*pVal;
	size_t nCurve,i;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	nCurve = EC_get_builtin_curves(0,0);
	if( nCurve < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	aCurve = (EC_builtin_curve *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(sxu32)(sizeof(EC_builtin_curve) * nCurve));
	if( aCurve == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( EC_get_builtin_curves(aCurve,nCurve) != nCurve ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( i = 0 ; i < nCurve ; ++i ){
		const char *zName = OBJ_nid2sn(aCurve[i].nid);
		if( zName == 0 ){
			continue;
		}
		ph7_value_string(pVal,zName,-1);
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,aCurve);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * openssl_get_cert_locations(): where the library looks for a CA bundle, plus
 * the two ini directives php lets a build override them with. Composer's
 * ca-bundle reads this to decide whether the system store is usable at all,
 * so the KEY SET is the contract even where the paths are the box's.
 */
static int vm_builtin_openssl_get_cert_locations(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pVal;
	const char *zIni;
	SyBlob sIni;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#define SSL_LOC(K,V) \
	ph7_value_string(pVal,(V) ? (V) : "",-1); \
	ph7_array_add_strkey_elem(pArray,K,pVal); \
	ph7_value_reset_string_cursor(pVal)
	SSL_LOC("default_cert_file",X509_get_default_cert_file());
	SSL_LOC("default_cert_file_env",X509_get_default_cert_file_env());
	SSL_LOC("default_cert_dir",X509_get_default_cert_dir());
	SSL_LOC("default_cert_dir_env",X509_get_default_cert_dir_env());
	SSL_LOC("default_private_dir",X509_get_default_private_dir());
	SSL_LOC("default_default_cert_area",X509_get_default_cert_area());
	PH7_VmIniGetStr(pCtx->pVm,"openssl.cafile",&sIni);
	SyBlobAppend(&sIni,"",1);
	zIni = (const char *)SyBlobData(&sIni);
	SSL_LOC("ini_cafile",zIni);
	SyBlobReset(&sIni);
	PH7_VmIniGetStr(pCtx->pVm,"openssl.capath",&sIni);
	SyBlobAppend(&sIni,"",1);
	zIni = (const char *)SyBlobData(&sIni);
	SSL_LOC("ini_capath",zIni);
	SyBlobRelease(&sIni);
#undef SSL_LOC
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Digests
 * ------------------------------------------------------------------------ */
static int vm_builtin_openssl_digest(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char aOut[EVP_MAX_MD_SIZE];
	unsigned int nOut = 0;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	EVP_MD_CTX *pMdCtx;
	const char *zData;
	int nData = 0,bRaw = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	pMd = PH7_SslDigestOfValue(apArg[1],&pFetched);
	if( pMd == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		bRaw = ph7_value_to_bool(apArg[2]);
	}
	pMdCtx = EVP_MD_CTX_new();
	if( pMdCtx == 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( EVP_DigestInit(pMdCtx,pMd) != 1
	 || EVP_DigestUpdate(pMdCtx,zData,(size_t)(nData > 0 ? nData : 0)) != 1
	 || EVP_DigestFinal(pMdCtx,aOut,&nOut) != 1 ){
		EVP_MD_CTX_free(pMdCtx);
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	EVP_MD_CTX_free(pMdCtx);
	if( pFetched ){ EVP_MD_free(pFetched); }
	if( bRaw ){
		ph7_result_string(pCtx,(const char *)aOut,(int)nOut);
	}else{
		char zHex[EVP_MAX_MD_SIZE * 2];
		static const char zDigit[] = "0123456789abcdef";
		unsigned int i;
		for( i = 0 ; i < nOut ; ++i ){
			zHex[i * 2]     = zDigit[(aOut[i] >> 4) & 0x0F];
			zHex[i * 2 + 1] = zDigit[aOut[i] & 0x0F];
		}
		ph7_result_string(pCtx,zHex,(int)(nOut * 2));
	}
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Ciphers
 * ------------------------------------------------------------------------ */
/*
 * What an AEAD cipher needs that an ordinary one does not. CCM is the awkward
 * one and php treats it separately at three points: its tag length must be set
 * BEFORE the key even when encrypting, the plaintext length has to be
 * declared in a first Update with a NULL buffer, and the whole message is one
 * run rather than a stream.
 */
typedef struct SslCipherMode SslCipherMode;
struct SslCipherMode {
	int bAead;
	int bSingleRun;      /* CCM: length declared up front, one Update */
	int bSetTagLength;   /* CCM: EVP_CTRL_AEAD_SET_TAG before the key */
};
static void SslLoadCipherMode(SslCipherMode *pMode,const EVP_CIPHER *pCipher)
{
	int iMode = EVP_CIPHER_get_mode(pCipher);
	pMode->bAead = 0;
	pMode->bSingleRun = 0;
	pMode->bSetTagLength = 0;
	switch( iMode ){
		case EVP_CIPH_GCM_MODE:
		case EVP_CIPH_OCB_MODE:
			pMode->bAead = 1;
			break;
		case EVP_CIPH_CCM_MODE:
			pMode->bAead = 1;
			pMode->bSingleRun = 1;
			pMode->bSetTagLength = 1;
			break;
		default:
			/* chacha20-poly1305 is AEAD without being one of the three MODES:
			 * OpenSSL files it as a stream cipher and marks it with a FLAG
			 * instead. Reading the mode alone answers `The tag cannot be used
			 * because the cipher algorithm does not support AEAD` for the one
			 * AEAD cipher a modern program is most likely to reach for. */
			if( (EVP_CIPHER_get_flags(pCipher) & EVP_CIPH_FLAG_AEAD_CIPHER) != 0 ){
				pMode->bAead = 1;
			}
			break;
	}
}
/*
 * php's IV screen, warnings and all -- and its POSITION, which a differential
 * sweep had to find: this runs AFTER the cipher is installed on the context
 * and its three sentences describe what it then DOES. A short IV is padded
 * with NULs, a long one is truncated, and neither is an error. An EMPTY one is
 * padded SILENTLY here; the "potentially insecure" warning about it is a
 * separate check that runs BEFORE the init (see SslWarnEmptyIv), which is why
 * a cipher this build cannot initialize at all -- `bf-cbc`, whose provider is
 * not loaded -- still raises that one sentence and none of these.
 *
 * The AEAD branch is not a screen at all: it is where php SETS the nonce
 * length, so an empty IV reaches EVP_CTRL_AEAD_SET_IVLEN as a zero and its
 * refusal is what a script sees.
 *
 * Answers 1 when *ppIv is a buffer the caller frees, 0 when it points into the
 * argument, and -1/-2 for a failure (-2 having already reported itself).
 */
static int SslPrepareIv(ph7_context *pCtx,EVP_CIPHER_CTX *pEvp,const EVP_CIPHER *pCipher,
	const char *zIv,int nIv,unsigned char **ppIv,int *pnIv,SslCipherMode *pMode)
{
	int nWant = EVP_CIPHER_get_iv_length(pCipher);
	unsigned char *zBuf;
	if( pMode->bAead ){
		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_IVLEN,nIv,0) != 1 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Setting of IV length for AEAD mode failed");
			return -2;
		}
		*ppIv = (unsigned char *)zIv;
		*pnIv = nIv;
		return 0;
	}
	if( nWant < 0 ){
		nWant = 0;
	}
	if( nIv == nWant ){
		*ppIv = nWant > 0 ? (unsigned char *)zIv : 0;
		*pnIv = nWant;
		return 0;
	}
	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nWant + 1));
	if( zBuf == 0 ){
		return -1;
	}
	SyZero(zBuf,(sxu32)(nWant + 1));
	if( nIv < 1 ){
		/* php's own "BC behavior": all zeros, and nothing said here. */
	}else if( nIv < nWant ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IV passed is only %d bytes long, cipher expects an IV of precisely %d bytes, padding with \\0",
			nIv,nWant);
		SyMemcpy(zIv,zBuf,(sxu32)nIv);
	}else{
		/* Including the nWant == 0 case: a cipher that takes NO iv still
		 * reports the one it was handed, and says it expected zero. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IV passed is %d bytes long which is longer than the %d expected by selected cipher, truncating",
			nIv,nWant);
		if( nWant > 0 ){
			SyMemcpy(zIv,zBuf,(sxu32)nWant);
		}
	}
	*ppIv = zBuf;
	*pnIv = nWant;
	return 1;   /* caller frees */
}
/*
 * The one IV sentence php raises BEFORE the cipher is installed: an ENCRYPT
 * with no IV at all, on a non-AEAD cipher that wants one.
 */
static void SslWarnEmptyIv(ph7_context *pCtx,const EVP_CIPHER *pCipher,int nIv,SslCipherMode *pMode)
{
	if( nIv == 0 && !pMode->bAead && EVP_CIPHER_get_iv_length(pCipher) > 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Using an empty Initialization Vector (iv) is potentially insecure and not recommended");
	}
}
/*
 * php's KEY screen. Three cases, and the third is the one a reading would
 * miss -- it was found by draining the error queue after every call in a
 * differential sweep:
 *
 *  - the passphrase is SHORTER than the cipher's key. php zero-pads it,
 *    unless OPENSSL_DONT_ZERO_PAD_KEY is set, which asks the CIPHER to take
 *    the short key instead and answers "Key length cannot be set for the
 *    cipher algorithm" for the fixed-length ciphers that refuse.
 *  - the passphrase is exactly the key length. Nothing happens.
 *  - the passphrase is LONGER. php still ASKS the cipher to take it, whatever
 *    the options say, and when the cipher refuses php ignores the refusal and
 *    uses the truncated key -- but the refusal is already in the error queue,
 *    so a successful `openssl_encrypt()` with a 40-byte passphrase leaves
 *    `error:03000082:digital envelope routines::invalid key length` for the
 *    next openssl_error_string(). That stray error IS php's answer.
 */
static int SslPrepareKey(ph7_context *pCtx,EVP_CIPHER_CTX *pEvp,
	const char *zPass,int nPass,int iOptions,unsigned char **ppKey,int *pbFreeKey)
{
	int nWant = EVP_CIPHER_CTX_get_key_length(pEvp);
	unsigned char *zBuf;
	*pbFreeKey = 0;
	if( nWant > nPass ){
		if( (iOptions & 4 /* OPENSSL_DONT_ZERO_PAD_KEY */)
		 && !EVP_CIPHER_CTX_set_key_length(pEvp,nPass) ){
			PH7_SslStoreErrors(pCtx->pVm);
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Key length cannot be set for the cipher algorithm");
			return -1;
		}
		zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nWant > 0 ? nWant : 1));
		if( zBuf == 0 ){
			return -2;
		}
		SyZero(zBuf,(sxu32)(nWant > 0 ? nWant : 1));
		if( nPass > 0 ){
			SyMemcpy(zPass,zBuf,(sxu32)nPass);
		}
		*ppKey = zBuf;
		*pbFreeKey = 1;
		return 0;
	}
	if( nPass > nWant && !EVP_CIPHER_CTX_set_key_length(pEvp,nPass) ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	*ppKey = (unsigned char *)zPass;
	return 0;
}
static int SslB64Consumer(const void *pData,unsigned int nLen,void *pUser)
{
	return SyBlobAppend((SyBlob *)pUser,pData,nLen);
}
static int vm_builtin_openssl_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData,*zMethod,*zPass,*zIv = "",*zAad = "";
	int nData = 0,nMethod = 0,nPass = 0,nIv = 0,nAad = 0;
	int iOptions = 0,iTagLen = 16,bFreeIv = 0,bFreeKey = 0,nIvUse = 0;
	sxi64 iOpt;
	const EVP_CIPHER *pCipher;
	EVP_CIPHER_CTX *pEvp = 0;
	EVP_CIPHER *pFetched = 0;
	SslCipherMode sMode;
	unsigned char *zKey = 0,*zIvUse = 0,*zOut = 0;
	unsigned char aTag[EVP_MAX_MD_SIZE];
	int nOut = 0,nFinal = 0,nMax,rc = 0;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	zMethod = ph7_value_to_string(apArg[1],&nMethod);
	zPass = ph7_value_to_string(apArg[2],&nPass);
	/* An EMPTY php string reads back as a NULL pointer here, and OpenSSL's
	 * Update refuses a NULL input even with a zero length -- which turned the
	 * round trip of an empty plaintext through an AEAD cipher into a false.
	 * See the `native-empty-string-slot-reads-null` note. */
	if( zData == 0 ){ zData = ""; }
	if( nArg > 3 ){
		iOpt = ph7_value_to_int64(apArg[3]);
		iOptions = (int)iOpt;
	}
	if( nArg > 4 ){
		zIv = ph7_value_to_string(apArg[4],&nIv);
	}
	if( nArg > 6 ){
		zAad = ph7_value_to_string(apArg[6],&nAad);
	}
	if( nArg > 7 ){
		iTagLen = (int)ph7_value_to_int64(apArg[7]);
	}
	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslLoadCipherMode(&sMode,pCipher);
	if( sMode.bAead && nArg > 5 && (iTagLen < 4 || iTagLen > 16) ){
		/* php reports an out-of-range tag length through the FAILURE of the
		 * retrieval rather than as its own screen, which is why a 0 and a 17
		 * both read "Retrieving verification tag failed". */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Retrieving verification tag failed");
		if( pFetched ){ EVP_CIPHER_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pEvp = EVP_CIPHER_CTX_new();
	if( pEvp == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		if( pFetched ){ EVP_CIPHER_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslWarnEmptyIv(pCtx,pCipher,nIv,&sMode);
	if( EVP_EncryptInit_ex(pEvp,pCipher,0,0,0) != 1 ){
		rc = -1;
		goto done;
	}
	bFreeIv = SslPrepareIv(pCtx,pEvp,pCipher,zIv,nIv,&zIvUse,&nIvUse,&sMode);
	if( bFreeIv < 0 ){
		rc = bFreeIv == -2 ? -2 : -1;
		goto done;
	}
	if( sMode.bSetTagLength ){
		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,iTagLen,0) != 1 ){
			rc = -1;
			goto done;
		}
	}
	if( SslPrepareKey(pCtx,pEvp,zPass,nPass,iOptions,&zKey,&bFreeKey) != 0 ){
		rc = -2;
		goto done;
	}
	if( EVP_EncryptInit_ex(pEvp,0,0,zKey,zIvUse) != 1 ){
		rc = -1;
		goto done;
	}
	if( iOptions & 2 /* OPENSSL_ZERO_PADDING */ ){
		EVP_CIPHER_CTX_set_padding(pEvp,0);
	}
	nMax = nData + EVP_CIPHER_get_block_size(pCipher);
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nMax > 0 ? nMax : 1));
	if( zOut == 0 ){
		rc = -2;
		goto done;
	}
	if( sMode.bSingleRun ){
		int nDummy = 0;
		if( EVP_EncryptUpdate(pEvp,0,&nDummy,0,nData) != 1 ){
			rc = -1;
			goto done;
		}
	}
	if( nAad > 0 && sMode.bAead ){
		int nDummy = 0;
		if( EVP_EncryptUpdate(pEvp,0,&nDummy,(const unsigned char *)zAad,nAad) != 1 ){
			rc = -1;
			goto done;
		}
	}
	if( EVP_EncryptUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1 ){
		rc = -1;
		goto done;
	}
	if( EVP_EncryptFinal_ex(pEvp,zOut + nOut,&nFinal) != 1 ){
		rc = -1;
		goto done;
	}
	nOut += nFinal;
	if( sMode.bAead && nArg > 5 ){
		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_GET_TAG,iTagLen,aTag) != 1 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Retrieving verification tag failed");
			rc = -2;
			goto done;
		}
		{
			ph7_value *pTag = ph7_context_new_scalar(pCtx);
			if( pTag ){
				ph7_value_string(pTag,(const char *)aTag,iTagLen);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pTag);
				ph7_context_release_value(pCtx,pTag);
			}
		}
	}else if( nArg > 5 ){
		/* A tag was ASKED for on a cipher that has none: php leaves the
		 * variable NULL and runs the encryption anyway -- it is the DECRYPT
		 * side that refuses a tag it cannot use. */
		ph7_value *pTag = ph7_context_new_scalar(pCtx);
		if( pTag ){
			ph7_value_null(pTag);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pTag);
			ph7_context_release_value(pCtx,pTag);
		}
	}
	if( iOptions & 1 /* OPENSSL_RAW_DATA */ ){
		ph7_result_string(pCtx,(const char *)zOut,nOut);
	}else{
		SyBlob sB64;
		SyBlobInit(&sB64,&pCtx->pVm->sAllocator);
		SyBase64Encode((const char *)zOut,(sxu32)nOut,SslB64Consumer,(void *)&sB64);
		ph7_result_string(pCtx,(const char *)SyBlobData(&sB64),(int)SyBlobLength(&sB64));
		SyBlobRelease(&sB64);
	}
done:
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( bFreeIv > 0 && zIvUse ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIvUse); }
	if( bFreeKey && zKey ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zKey); }
	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }
	if( pFetched ){ EVP_CIPHER_free(pFetched); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
static int vm_builtin_openssl_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zData,*zMethod,*zPass,*zIv = "",*zAad = "",*zTag = 0;
	int nData = 0,nMethod = 0,nPass = 0,nIv = 0,nAad = 0,nTag = 0;
	int iOptions = 0,bFreeIv = 0,bFreeKey = 0,nIvUse = 0,bHasTag = 0;
	const EVP_CIPHER *pCipher;
	EVP_CIPHER_CTX *pEvp = 0;
	EVP_CIPHER *pFetched = 0;
	SslCipherMode sMode;
	SyBlob sRaw;
	unsigned char *zKey = 0,*zIvUse = 0,*zOut = 0;
	int nOut = 0,nFinal = 0,rc = 0,bRawInit = 0;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	zMethod = ph7_value_to_string(apArg[1],&nMethod);
	zPass = ph7_value_to_string(apArg[2],&nPass);
	if( zData == 0 ){ zData = ""; }   /* see openssl_encrypt() above */
	if( nArg > 3 ){
		iOptions = (int)ph7_value_to_int64(apArg[3]);
	}
	if( nArg > 4 ){
		zIv = ph7_value_to_string(apArg[4],&nIv);
	}
	if( nArg > 5 && !ph7_value_is_null(apArg[5]) ){
		zTag = ph7_value_to_string(apArg[5],&nTag);
		bHasTag = 1;
	}
	if( nArg > 6 ){
		zAad = ph7_value_to_string(apArg[6],&nAad);
	}
	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslLoadCipherMode(&sMode,pCipher);
	SyBlobInit(&sRaw,&pCtx->pVm->sAllocator);
	bRawInit = 1;
	if( (iOptions & 1 /* OPENSSL_RAW_DATA */) == 0 ){
		if( nData > 0 ){
			SyBase64Decode(zData,(sxu32)nData,SslB64Consumer,(void *)&sRaw);
		}
		zData = (const char *)SyBlobData(&sRaw);
		nData = (int)SyBlobLength(&sRaw);
		if( zData == 0 ){ zData = ""; }
	}
	pEvp = EVP_CIPHER_CTX_new();
	if( pEvp == 0 ){
		rc = -1;
		goto done;
	}
	if( EVP_DecryptInit_ex(pEvp,pCipher,0,0,0) != 1 ){
		rc = -1;
		goto done;
	}
	bFreeIv = SslPrepareIv(pCtx,pEvp,pCipher,zIv,nIv,&zIvUse,&nIvUse,&sMode);
	if( bFreeIv < 0 ){
		rc = bFreeIv == -2 ? -2 : -1;
		goto done;
	}
	if( bHasTag && !sMode.bAead ){
		/* NOT a refusal, which is the whole point of this line: php says the
		 * tag cannot be used and then decrypts anyway, so a correct call that
		 * happens to pass a tag still answers its plaintext. It also sits
		 * between the IV screen and the key one -- a short IV reports first,
		 * a bad key reports after. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"The tag cannot be used because the cipher algorithm does not support AEAD");
	}
	if( sMode.bSetTagLength ){
		/* CCM wants the tag AND its length before the key: the mode carries no
		 * Final, so the value it will verify against has to be on the context
		 * by the time the key is installed. GCM and OCB take theirs after. */
		if( nTag < 1 ){
			rc = -2;
			goto done;
		}
		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,nTag,(void *)zTag) != 1 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Setting tag for AEAD cipher decryption failed");
			rc = -1;
			goto done;
		}
	}
	if( SslPrepareKey(pCtx,pEvp,zPass,nPass,iOptions,&zKey,&bFreeKey) != 0 ){
		rc = -2;
		goto done;
	}
	if( EVP_DecryptInit_ex(pEvp,0,0,zKey,zIvUse) != 1 ){
		rc = -1;
		goto done;
	}
	if( sMode.bAead && !sMode.bSetTagLength ){
		if( nTag < 1 ){
			/* No tag at all on an AEAD cipher: a flat false, no message and
			 * nothing left in the error ring. php raises no `A tag should be
			 * provided` sentence here. */
			rc = -2;
			goto done;
		}
		if( EVP_CIPHER_CTX_ctrl(pEvp,EVP_CTRL_AEAD_SET_TAG,nTag,(void *)zTag) != 1 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Setting tag for AEAD cipher decryption failed");
			rc = -1;
			goto done;
		}
	}
	if( iOptions & 2 /* OPENSSL_ZERO_PADDING */ ){
		EVP_CIPHER_CTX_set_padding(pEvp,0);
	}
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));
	if( zOut == 0 ){
		rc = -2;
		goto done;
	}
	if( sMode.bSingleRun ){
		int nDummy = 0;
		if( EVP_DecryptUpdate(pEvp,0,&nDummy,0,nData) != 1 ){
			rc = -1;
			goto done;
		}
	}
	if( nAad > 0 && sMode.bAead ){
		int nDummy = 0;
		if( EVP_DecryptUpdate(pEvp,0,&nDummy,(const unsigned char *)zAad,nAad) != 1 ){
			rc = -1;
			goto done;
		}
	}
	if( EVP_DecryptUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1 ){
		rc = -1;
		goto done;
	}
	if( sMode.bSingleRun ){
		/* CCM verifies in the Update; there is no Final to run. */
		ph7_result_string(pCtx,(const char *)zOut,nOut);
		goto done;
	}
	if( EVP_DecryptFinal_ex(pEvp,zOut + nOut,&nFinal) != 1 ){
		rc = -1;
		goto done;
	}
	nOut += nFinal;
	ph7_result_string(pCtx,(const char *)zOut,nOut);
done:
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( bFreeIv > 0 && zIvUse ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIvUse); }
	if( bFreeKey && zKey ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zKey); }
	if( bRawInit ){ SyBlobRelease(&sRaw); }
	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }
	if( pFetched ){ EVP_CIPHER_free(pFetched); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * The two cipher-length questions. They differ from openssl_encrypt()'s
 * screen in one way that is php's and not obvious: an EMPTY name here is a
 * ValueError ("must not be empty") where openssl_encrypt() calls the same
 * empty name an unknown algorithm and warns.
 */
static int SslCipherLength(ph7_context *pCtx,int nArg,ph7_value **apArg,int bKey)
{
	const char *zName;
	int nName = 0,nLen;
	const EVP_CIPHER *pCipher;
	EVP_CIPHER *pFetched = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nName < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($cipher_algo) must not be empty",ph7_function_name(pCtx));
	}
	pCipher = SslCipherByName(zName,&pFetched);
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = bKey ? EVP_CIPHER_get_key_length(pCipher) : EVP_CIPHER_get_iv_length(pCipher);
	if( pFetched ){
		EVP_CIPHER_free(pFetched);
	}
	ph7_result_int(pCtx,nLen);
	return PH7_OK;
}
static int vm_builtin_openssl_cipher_iv_length(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslCipherLength(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cipher_key_length(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslCipherLength(pCtx,nArg,apArg,1);
}

/* ------------------------------------------------------------------------
 * Randomness and key derivation
 * ------------------------------------------------------------------------ */
/*
 * openssl_random_pseudo_bytes(): php's `$strong_result` is written on SUCCESS
 * only, and it has been a constant true since php dropped the pseudo-random
 * fallback -- RAND_bytes either produces cryptographic bytes or fails.
 */
static int vm_builtin_openssl_random_pseudo_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iLen;
	unsigned char *zBuf;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iLen = ph7_value_to_int64(apArg[0]);
	if( iLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($length) must be greater than 0",ph7_function_name(pCtx));
	}
	if( iLen > SXI32_HIGH ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($length) must be less than or equal to %d",
			ph7_function_name(pCtx),SXI32_HIGH);
	}
	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);
	if( zBuf == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( RAND_bytes(zBuf,(int)iLen) != 1 ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);
		PH7_SslStoreErrors(pCtx->pVm);
		return PH7_VmThrowException(pCtx,"Exception","%s(): Cannot generate a random string",
			ph7_function_name(pCtx));
	}
	if( nArg > 1 ){
		ph7_value *pStrong = ph7_context_new_scalar(pCtx);
		if( pStrong ){
			ph7_value_bool(pStrong,1);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pStrong);
			ph7_context_release_value(pCtx,pStrong);
		}
	}
	ph7_result_string(pCtx,(const char *)zBuf,(int)iLen);
	SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);
	return PH7_OK;
}
/*
 * openssl_pbkdf2(). Two screens are php's own ValueErrors and the third is
 * not a screen at all: a zero ITERATION count is a flat false with no message,
 * because php hands it to PKCS5_PBKDF2_HMAC and reports what that refuses.
 */
static int vm_builtin_openssl_pbkdf2(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPass,*zSalt;
	int nPass = 0,nSalt = 0;
	sxi64 iKeyLen,iIter;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	unsigned char *zOut;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPass = ph7_value_to_string(apArg[0],&nPass);
	zSalt = ph7_value_to_string(apArg[1],&nSalt);
	iKeyLen = ph7_value_to_int64(apArg[2]);
	iIter = ph7_value_to_int64(apArg[3]);
	if( iKeyLen < 1 || iKeyLen > SXI32_HIGH ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #3 ($key_length) must be greater than 0",ph7_function_name(pCtx));
	}
	if( iIter > SXI32_HIGH ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #4 ($iterations) must be less than or equal to %d",
			ph7_function_name(pCtx),SXI32_HIGH);
	}
	if( nArg > 4 ){
		pMd = PH7_SslDigestOfValue(apArg[4],&pFetched);
	}else{
		pMd = EVP_sha1();
	}
	if( pMd == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iKeyLen);
	if( zOut == 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PKCS5_PBKDF2_HMAC(zPass,nPass,(const unsigned char *)zSalt,nSalt,
			(int)iIter,pMd,(int)iKeyLen,zOut) != 1 ){
		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pFetched ){ EVP_MD_free(pFetched); }
	ph7_result_string(pCtx,(const char *)zOut,(int)iKeyLen);
	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);
	return PH7_OK;
}


/* ------------------------------------------------------------------------
 * Keys
 * ------------------------------------------------------------------------ */
/*
 * A `$key` argument's four shapes, and what each means. php declares these
 * parameters UNTYPED because there is no type that covers them:
 *
 *   an OpenSSLAsymmetricKey       -- the key itself
 *   an OpenSSLCertificate         -- its SUBJECT's public key (public doors only)
 *   a string                      -- PEM or DER bytes, or a `file://` path
 *   an array [$key, $passphrase]  -- a private key and what unlocks it
 *
 * A certificate PEM is also accepted as a public key STRING, which is what
 * lets `openssl_verify($data, $sig, $certPem)` work -- the ordinary spelling
 * in a program that has a certificate rather than a bare key.
 *
 * Answers 0 for anything it cannot read, having left OpenSSL's own reason in
 * the error ring; the CALLER supplies php's sentence, since the five doors
 * word it five different ways ("cannot be coerced into a private key", "key
 * parameter is not a valid public key", "Unable to coerce parameter 4 into a
 * private key", ...). *pbOwn says whether the caller frees it.
 */
/*
 * Does this key carry its PRIVATE half? php asks the key rather than
 * remembering how it was made, so a key read out of a private PEM is private
 * whichever door produced the object. The three probes are the three shapes a
 * private component takes: RSA's `d`, the finite-field/EC private BIGNUM, and
 * the raw octet string an Edwards/Montgomery key keeps. Each miss pushes an
 * error, so the whole check runs between a MARK and a pop -- php's own answer
 * comes from inspecting the structure and leaves the error ring alone.
 */
static int SslKeyIsPrivate(EVP_PKEY *pKey)
{
	BIGNUM *pBn = 0;
	size_t nOct = 0;
	int bPriv = 0;
	if( pKey == 0 ){
		return 0;
	}
	ERR_set_mark();
	if( EVP_PKEY_get_bn_param(pKey,OSSL_PKEY_PARAM_RSA_D,&pBn) == 1 ){
		bPriv = 1;
	}else if( EVP_PKEY_get_bn_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,&pBn) == 1 ){
		bPriv = 1;
	}else if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,0,0,&nOct) == 1 ){
		bPriv = 1;
	}
	if( pBn ){
		BN_clear_free(pBn);
	}
	ERR_pop_to_mark();
	return bPriv;
}
static const char * SslFilePrefix(const char *zStr,int nStr,int *pnRest)
{
	static const char zPfx[] = "file://";
	int nPfx = (int)(sizeof(zPfx) - 1);
	if( nStr <= nPfx || SyStrnicmp(zStr,zPfx,(sxu32)nPfx) != 0 ){
		return 0;
	}
	*pnRest = nStr - nPfx;
	return zStr + nPfx;
}
/*
 * The bytes behind a string argument: either the argument itself or, for a
 * `file://` spelling, what that file holds. Reads through the engine's stream
 * layer, so a userland wrapper and a phar both answer.
 */
PH7_PRIVATE int PH7_SslBytesOfValue(ph7_context *pCtx,ph7_value *pVal,SyBlob *pOut,
	const char **pzData,int *pnData)
{
	const char *zStr,*zPath;
	int nStr = 0,nPath = 0;
	const ph7_io_stream *pStream;
	void *pHandle;
	zStr = ph7_value_to_string(pVal,&nStr);
	if( zStr == 0 ){
		zStr = "";
	}
	zPath = SslFilePrefix(zStr,nStr,&nPath);
	if( zPath == 0 ){
		*pzData = zStr;
		*pnData = nStr;
		return 0;
	}
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nPath);
	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,0) : 0;
	if( pHandle == 0 ){
		return -1;
	}
	PH7_StreamReadWholeFile(pHandle,pStream,pOut);
	PH7_StreamCloseHandle(pStream,pHandle);
	*pzData = (const char *)SyBlobData(pOut);
	*pnData = (int)SyBlobLength(pOut);
	if( *pzData == 0 ){
		*pzData = "";
	}
	return 0;
}
/* The nth VALUE of an array, in insertion order -- what php's positional
 * reads of `[$key, $passphrase]` and of openssl_seal()'s recipient list mean.
 * The engine has no by-index array accessor, so the walk is spelled here. */
static ph7_value * SslArrayNth(ph7_value *pArray,int n)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 i;
	if( pArray == 0 || !ph7_value_is_array(pArray) || n < 0 ){
		return 0;
	}
	pMap = (ph7_hashmap *)pArray->x.pOther;
	pEntry = pMap->pFirst;
	for( i = 0 ; i < pMap->nEntry ; ++i ){
		if( (int)i == n ){
			return HashmapExtractNodeValue(pEntry);
		}
		pEntry = pEntry->pPrev;   /* forward walk (reverse link) */
	}
	return 0;
}
/*
 * The `_to_file` half of the five export doors. It writes through OpenSSL's
 * own file BIO rather than the engine's stream layer, and that is php's choice
 * showing through rather than a shortcut: a path that cannot be opened leaves
 * `system library::No such file or directory` and `BIO routines::no such file`
 * in the error ring, which is what a script reads back. The cost is the same
 * one php pays -- these doors write to the filesystem and to nothing else, so
 * a `phar://` or userland-wrapper path is not a destination here.
 */
PH7_PRIVATE int PH7_SslWriteFileArg(ph7_context *pCtx,const char *zPath,int nPath,
	const void *pData,sxu32 nData)
{
	char *zZ;
	BIO *pOut;
	int rc = -1;
	if( zPath == 0 || nPath < 1 ){
		return -1;
	}
	zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));
	if( zZ == 0 ){
		return -1;
	}
	SyMemcpy(zPath,zZ,(sxu32)nPath);
	zZ[nPath] = 0;
	pOut = BIO_new_file(zZ,"wb");
	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
	if( pOut == 0 ){
		return -1;
	}
	if( nData == 0 || BIO_write(pOut,pData,(int)nData) == (int)nData ){
		rc = 0;
	}
	BIO_free(pOut);
	return rc;
}
/* Read a whole file through the engine's stream layer -- the certificate
 * unit's `$ca_info` paths and the PKCS#7 input files. */
PH7_PRIVATE int PH7_SslReadFileArg(ph7_context *pCtx,const char *zPath,int nPath,SyBlob *pOut)
{
	const ph7_io_stream *pStream;
	void *pHandle;
	const char *zTarget = zPath;
	if( zPath == 0 || nPath < 1 ){
		return -1;
	}
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTarget,nPath);
	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTarget,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,0) : 0;
	if( pHandle == 0 ){
		return -1;
	}
	PH7_StreamReadWholeFile(pHandle,pStream,pOut);
	PH7_StreamCloseHandle(pStream,pHandle);
	return 0;
}
/* One NUL-terminated copy of a passphrase, for OpenSSL's default password
 * callback (which takes `u` as a C string when no callback is given). */
static char * SslPassCopy(ph7_context *pCtx,const char *zPass,int nPass)
{
	char *zBuf;
	if( zPass == 0 || nPass < 1 ){
		return 0;
	}
	zBuf = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));
	if( zBuf == 0 ){
		return 0;
	}
	SyMemcpy(zPass,zBuf,(sxu32)nPass);
	zBuf[nPass] = 0;
	return zBuf;
}
/*
 * The passphrase callback. It is ALWAYS installed, even when there is no
 * passphrase, and that is the point: OpenSSL's default callback PROMPTS on the
 * terminal when it is handed nothing, so an encrypted key opened without a
 * passphrase would stop a CLI program dead waiting for a human. php inherits
 * that prompt; this answers the empty string instead, which is the same
 * `false` php gives when its input is not a terminal.
 */
static int SslPassCb(char *zBuf,int nBuf,int rwflag,void *pUser)
{
	const char *zPass = (const char *)pUser;
	int n = zPass ? (int)SyStrlen(zPass) : 0;
	SXUNUSED(rwflag);
	if( n > nBuf ){
		n = nBuf;
	}
	if( n > 0 ){
		SyMemcpy(zPass,zBuf,(sxu32)n);
	}
	return n;
}
static EVP_PKEY * SslKeyFromBytes(const char *zData,int nData,int bPublic,char *zPass)
{
	BIO *pBio;
	EVP_PKEY *pKey = 0;
	if( nData < 1 ){
		return 0;
	}
	pBio = BIO_new_mem_buf(zData,nData);
	if( pBio == 0 ){
		return 0;
	}
	/* PEM only, both halves: a string holding DER is
	 * `error:1E08010C:DECODER routines::unsupported` and false under php, and
	 * a DER key reaches these doors through a `file://` path instead. */
	if( bPublic ){
		pKey = PEM_read_bio_PUBKEY(pBio,0,0,0);
		if( pKey == 0 ){
			/* A certificate is a public key a program HAS, so php reads one
			 * here too -- `openssl_verify($d, $s, $certPem)` is the ordinary
			 * spelling in a program that was handed a certificate. */
			X509 *pCert;
			ERR_set_mark();
			BIO_reset(pBio);
			pCert = PEM_read_bio_X509(pBio,0,0,0);
			if( pCert ){
				ERR_clear_last_mark();
				pKey = X509_get_pubkey(pCert);
				X509_free(pCert);
			}else{
				ERR_pop_to_mark();
			}
		}
	}else{
		pKey = PEM_read_bio_PrivateKey(pBio,0,SslPassCb,(void *)zPass);
	}
	BIO_free(pBio);
	return pKey;
}
PH7_PRIVATE EVP_PKEY * PH7_SslKeyOfValue(ph7_context *pCtx,ph7_value *pVal,int bPublic,int *pbOwn)
{
	SyBlob sFile;
	const char *zData = 0,*zPass = 0;
	int nData = 0,nPass = 0,rc;
	char *zPassZ = 0;
	EVP_PKEY *pKey = 0;
	ph7_value *pKeyVal = pVal;
	*pbOwn = 0;
	if( pVal == 0 ){
		return 0;
	}
	if( ph7_value_is_object(pVal) ){
		void *pHandle = PH7_SslHandleOf(pVal,PHL_SSL_KIND_KEY);
		if( pHandle ){
			/* A key OBJECT carries its half with it, and php checks: a private
			 * key at a public door and a public key at a private one are both
			 * refusals, each with its own sentence, and each is followed by
			 * the CALLER's own. A key STRING gets neither message -- it simply
			 * fails to parse -- which is why these two live here rather than
			 * in the byte reader. */
			int bPriv = SslKeyIsPrivate((EVP_PKEY *)pHandle);
			if( bPublic && bPriv ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Don't know how to get public key from this private key");
				return 0;
			}
			if( !bPublic && !bPriv ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Supplied key param is a public key");
				return 0;
			}
			return (EVP_PKEY *)pHandle;
		}
		pHandle = PH7_SslHandleOf(pVal,PHL_SSL_KIND_CERT);
		if( pHandle && bPublic ){
			EVP_PKEY *pPub = X509_get_pubkey((X509 *)pHandle);
			if( pPub ){
				*pbOwn = 1;
			}
			return pPub;
		}
		return 0;
	}
	if( ph7_value_is_array(pVal) ){
		/* php's [$key, $passphrase] pair, and it is a SHAPE rather than a
		 * position: the keys 0 and 1 must both be there. An `['key' => …,
		 * 'pass' => …]` spelling is `ValueError: Key array must be of the form
		 * array(0 => key, 1 => phrase)` -- a bare sentence with no function
		 * name in front of it, unlike every other diagnostic here -- and so is
		 * an array of one. A THIRD element is ignored. The check runs before
		 * the public/private split, so a pair handed to a public door throws
		 * the same thing rather than being refused as "not a public key". */
		ph7_value *pFirst = ph7_array_fetch(pVal,"0",-1);
		ph7_value *pSecond = ph7_array_fetch(pVal,"1",-1);
		if( pFirst == 0 || pSecond == 0 ){
			*pbOwn = -1;   /* the caller raises php's ValueError */
			return 0;
		}
		if( bPublic ){
			return 0;
		}
		if( ph7_value_is_object(pFirst) ){
			return PH7_SslKeyOfValue(pCtx,pFirst,bPublic,pbOwn);
		}
		if( !ph7_value_is_null(pSecond) ){
			zPass = ph7_value_to_string(pSecond,&nPass);
		}
		pKeyVal = pFirst;
	}
	if( !ph7_value_is_string(pKeyVal) ){
		return 0;
	}
	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
	rc = PH7_SslBytesOfValue(pCtx,pKeyVal,&sFile,&zData,&nData);
	if( rc == 0 ){
		zPassZ = SslPassCopy(pCtx,zPass,nPass);
		pKey = SslKeyFromBytes(zData,nData,bPublic,zPassZ);
		if( zPassZ ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ);
		}
	}
	SyBlobRelease(&sFile);
	if( pKey ){
		*pbOwn = 1;
	}else{
		PH7_SslStoreErrors(pCtx->pVm);
	}
	return pKey;
}
/* The private-key door with php's separate `$passphrase` argument (rather than
 * the array pair): openssl_pkey_get_private() and openssl_pkey_export(). */
static EVP_PKEY * SslPrivateKeyWithPass(ph7_context *pCtx,ph7_value *pVal,
	const char *zPass,int nPass,int *pbOwn)
{
	SyBlob sFile;
	const char *zData = 0;
	char *zPassZ;
	EVP_PKEY *pKey = 0;
	int nData = 0;
	*pbOwn = 0;
	if( pVal == 0 ){
		return 0;
	}
	if( ph7_value_is_object(pVal) || ph7_value_is_array(pVal) || zPass == 0 || nPass < 1 ){
		return PH7_SslKeyOfValue(pCtx,pVal,0,pbOwn);
	}
	if( !ph7_value_is_string(pVal) ){
		return 0;
	}
	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){
		zPassZ = SslPassCopy(pCtx,zPass,nPass);
		pKey = SslKeyFromBytes(zData,nData,0,zPassZ);
		if( zPassZ ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ);
		}
	}
	SyBlobRelease(&sFile);
	if( pKey ){
		*pbOwn = 1;
	}else{
		PH7_SslStoreErrors(pCtx->pVm);
	}
	return pKey;
}
static void SslReleaseKey(EVP_PKEY *pKey,int bOwn)
{
	if( pKey && bOwn > 0 ){
		EVP_PKEY_free(pKey);
	}
}
/* php's bare ValueError for a malformed key pair. Every door that takes a
 * `$key` reports it, and none of them puts its own name in front. */
PH7_PRIVATE int PH7_SslArrayShapeError(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"ValueError",
		"Key array must be of the form array(0 => key, 1 => phrase)");
}
/* ---- openssl_pkey_new() ---- */
/*
 * php's key TYPE numbering is its own (OPENSSL_KEYTYPE_*), and the four
 * generated types map onto OpenSSL's names. The four that only appear in
 * get_details -- X25519, ED25519, X448, ED448 -- are read but never
 * GENERATED by php's `private_key_type`, which is why they are absent here.
 */
static int SslKeyTypeOf(EVP_PKEY *pKey)
{
	switch( EVP_PKEY_get_base_id(pKey) ){
		case EVP_PKEY_RSA:
		case EVP_PKEY_RSA_PSS: return 0;   /* OPENSSL_KEYTYPE_RSA */
		case EVP_PKEY_DSA:     return 1;
		case EVP_PKEY_DH:      return 2;
		case EVP_PKEY_EC:      return 3;
		case EVP_PKEY_X25519:  return 4;
		case EVP_PKEY_ED25519: return 5;
		case EVP_PKEY_X448:    return 6;
		case EVP_PKEY_ED448:   return 7;
		default:               return -1;
	}
}
/* One BIGNUM out of an options sub-array, as php reads it: a binary string in
 * network byte order. */
static BIGNUM * SslBnFromArray(ph7_value *pArray,const char *zKey)
{
	ph7_value *pVal = pArray ? ph7_array_fetch(pArray,zKey,-1) : 0;
	const char *zRaw;
	int nRaw = 0;
	if( pVal == 0 || !ph7_value_is_string(pVal) ){
		return 0;
	}
	zRaw = ph7_value_to_string(pVal,&nRaw);
	if( zRaw == 0 || nRaw < 1 ){
		return 0;
	}
	return BN_bin2bn((const unsigned char *)zRaw,nRaw,0);
}
static int SslBldPush(OSSL_PARAM_BLD *pBld,const char *zName,BIGNUM *pBn)
{
	return pBn ? OSSL_PARAM_BLD_push_BN(pBld,zName,pBn) : 0;
}
/*
 * A key built from its COMPONENTS -- `openssl_pkey_new(['rsa' => [...]])` and
 * its three siblings. php requires the PRIVATE half in every one of them: an
 * `['rsa' => ['n' => …, 'e' => …]]` with no `d` is a flat false rather than a
 * public key, which is the one thing a caller who reads the manual would get
 * wrong.
 */
static EVP_PKEY * SslKeyFromComponents(ph7_context *pCtx,ph7_value *pOpts)
{
	static const struct { const char *zGroup; const char *zType; } aKind[] = {
		{ "rsa", "RSA" }, { "dsa", "DSA" }, { "dh", "DH" }, { "ec", "EC" }
	};
	ph7_value *pGroup = 0;
	const char *zType = 0;
	OSSL_PARAM_BLD *pBld;
	OSSL_PARAM *pParams = 0;
	EVP_PKEY_CTX *pKCtx = 0;
	EVP_PKEY *pKey = 0;
	BIGNUM *aBn[8];
	int nBn = 0,i,bOk = 0;
	unsigned char *zPoint = 0;
	for( i = 0 ; i < (int)SX_ARRAYSIZE(aKind) ; ++i ){
		pGroup = ph7_array_fetch(pOpts,aKind[i].zGroup,-1);
		if( pGroup && ph7_value_is_array(pGroup) ){
			zType = aKind[i].zType;
			break;
		}
		pGroup = 0;
	}
	if( zType == 0 ){
		return 0;
	}
	pBld = OSSL_PARAM_BLD_new();
	if( pBld == 0 ){
		return 0;
	}
	SyZero(aBn,sizeof(aBn));
	if( SslStrCmp(zType,"RSA") == 0 ){
		static const struct { const char *zPhp; const char *zParam; int bReq; } aRsa[] = {
			{ "n",    OSSL_PKEY_PARAM_RSA_N,           1 },
			{ "e",    OSSL_PKEY_PARAM_RSA_E,           1 },
			{ "d",    OSSL_PKEY_PARAM_RSA_D,           1 },
			{ "p",    OSSL_PKEY_PARAM_RSA_FACTOR1,     0 },
			{ "q",    OSSL_PKEY_PARAM_RSA_FACTOR2,     0 },
			{ "dmp1", OSSL_PKEY_PARAM_RSA_EXPONENT1,   0 },
			{ "dmq1", OSSL_PKEY_PARAM_RSA_EXPONENT2,   0 },
			{ "iqmp", OSSL_PKEY_PARAM_RSA_COEFFICIENT1,0 }
		};
		bOk = 1;
		for( i = 0 ; i < (int)SX_ARRAYSIZE(aRsa) ; ++i ){
			BIGNUM *pBn = SslBnFromArray(pGroup,aRsa[i].zPhp);
			if( pBn == 0 ){
				if( aRsa[i].bReq ){ bOk = 0; }
				continue;
			}
			aBn[nBn++] = pBn;
			if( !SslBldPush(pBld,aRsa[i].zParam,pBn) ){ bOk = 0; }
		}
	}else if( SslStrCmp(zType,"EC") == 0 ){
		ph7_value *pName = ph7_array_fetch(pGroup,"curve_name",-1);
		BIGNUM *pX = SslBnFromArray(pGroup,"x");
		BIGNUM *pY = SslBnFromArray(pGroup,"y");
		BIGNUM *pD = SslBnFromArray(pGroup,"d");
		int nCurve = 0;
		const char *zCurve = pName ? ph7_value_to_string(pName,&nCurve) : 0;
		if( pX ){ aBn[nBn++] = pX; }
		if( pY ){ aBn[nBn++] = pY; }
		if( pD ){ aBn[nBn++] = pD; }
		if( zCurve && nCurve > 0 ){
			bOk = OSSL_PARAM_BLD_push_utf8_string(pBld,OSSL_PKEY_PARAM_GROUP_NAME,zCurve,(size_t)nCurve);
		}
		if( bOk && pD ){
			bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PRIV_KEY,pD);
		}else{
			bOk = 0;
		}
		if( bOk && pX && pY ){
			/* An EC public key is the ENCODED POINT rather than a pair of
			 * numbers: 0x04 followed by X and Y, each padded to the field's
			 * width. */
			EC_GROUP *pGrp = EC_GROUP_new_by_curve_name(OBJ_sn2nid(zCurve));
			int nField = pGrp ? (EC_GROUP_get_degree(pGrp) + 7) / 8 : 0;
			if( nField > 0 ){
				zPoint = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
					(sxu32)(1 + 2 * nField));
				if( zPoint ){
					SyZero(zPoint,(sxu32)(1 + 2 * nField));
					zPoint[0] = 0x04;
					BN_bn2binpad(pX,zPoint + 1,nField);
					BN_bn2binpad(pY,zPoint + 1 + nField,nField);
					bOk = OSSL_PARAM_BLD_push_octet_string(pBld,OSSL_PKEY_PARAM_PUB_KEY,
						zPoint,(size_t)(1 + 2 * nField));
				}else{
					bOk = 0;
				}
			}else{
				bOk = 0;
			}
			if( pGrp ){
				EC_GROUP_free(pGrp);
			}
		}
	}else{
		/* DSA and DH share the finite-field parameter names; DH has no q. */
		BIGNUM *pP = SslBnFromArray(pGroup,"p");
		BIGNUM *pQ = SslBnFromArray(pGroup,"q");
		BIGNUM *pG = SslBnFromArray(pGroup,"g");
		BIGNUM *pPriv = SslBnFromArray(pGroup,"priv_key");
		BIGNUM *pPub = SslBnFromArray(pGroup,"pub_key");
		if( pP ){ aBn[nBn++] = pP; }
		if( pQ ){ aBn[nBn++] = pQ; }
		if( pG ){ aBn[nBn++] = pG; }
		if( pPriv ){ aBn[nBn++] = pPriv; }
		if( pPub ){ aBn[nBn++] = pPub; }
		bOk = pP && pG && pPriv;
		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_P,pP); }
		if( bOk && pQ ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_Q,pQ); }
		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_FFC_G,pG); }
		if( bOk ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PRIV_KEY,pPriv); }
		if( bOk && pPub ){ bOk = SslBldPush(pBld,OSSL_PKEY_PARAM_PUB_KEY,pPub); }
	}
	if( bOk ){
		pParams = OSSL_PARAM_BLD_to_param(pBld);
	}
	if( pParams ){
		pKCtx = EVP_PKEY_CTX_new_from_name(0,zType,0);
	}
	if( pKCtx && EVP_PKEY_fromdata_init(pKCtx) > 0 ){
		if( EVP_PKEY_fromdata(pKCtx,&pKey,EVP_PKEY_KEYPAIR,pParams) <= 0 ){
			pKey = 0;
		}
	}
	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
	if( pParams ){ OSSL_PARAM_free(pParams); }
	OSSL_PARAM_BLD_free(pBld);
	for( i = 0 ; i < nBn ; ++i ){ BN_free(aBn[i]); }
	if( zPoint ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPoint); }
	if( pKey == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	return pKey;
}
/*
 * php's `private_key_type` covers all EIGHT of its OPENSSL_KEYTYPE_* numbers,
 * not just the four a reading of the manual suggests: 4..7 are X25519,
 * ED25519, X448 and ED448, and each generates without a bit length or a curve.
 * Anything outside 0..7 is `Unsupported private key type`.
 *
 * The three FAMILIES need three different generation shapes, and getting that
 * wrong is a silent false rather than an error: RSA takes its size as a keygen
 * parameter, DSA and DH need their DOMAIN PARAMETERS generated first and the
 * key generated from those, and EC/Edwards take a group name or nothing at all.
 */
static const char * SslKeyTypeName(sxi64 iType)
{
	switch( iType ){
		case 0: return "RSA";
		case 1: return "DSA";
		case 2: return "DH";
		case 3: return "EC";
		case 4: return "X25519";
		case 5: return "ED25519";
		case 6: return "X448";
		case 7: return "ED448";
		default: return 0;
	}
}
static EVP_PKEY * SslGenFfcKey(const char *zType,int nBits)
{
	EVP_PKEY_CTX *pCtx1,*pCtx2;
	EVP_PKEY *pParams = 0,*pKey = 0;
	OSSL_PARAM aP[2];
	unsigned int nB = (unsigned int)nBits;
	pCtx1 = EVP_PKEY_CTX_new_from_name(0,zType,0);
	if( pCtx1 == 0 ){
		return 0;
	}
	aP[0] = OSSL_PARAM_construct_uint(OSSL_PKEY_PARAM_FFC_PBITS,&nB);
	aP[1] = OSSL_PARAM_construct_end();
	if( EVP_PKEY_paramgen_init(pCtx1) <= 0
	 || EVP_PKEY_CTX_set_params(pCtx1,aP) <= 0
	 || EVP_PKEY_paramgen(pCtx1,&pParams) <= 0 ){
		EVP_PKEY_CTX_free(pCtx1);
		return 0;
	}
	EVP_PKEY_CTX_free(pCtx1);
	pCtx2 = EVP_PKEY_CTX_new(pParams,0);
	if( pCtx2 && EVP_PKEY_keygen_init(pCtx2) > 0 ){
		if( EVP_PKEY_keygen(pCtx2,&pKey) <= 0 ){
			pKey = 0;
		}
	}
	if( pCtx2 ){ EVP_PKEY_CTX_free(pCtx2); }
	EVP_PKEY_free(pParams);
	return pKey;
}
/*
 * The generator behind openssl_pkey_new() and openssl_csr_new()'s "no key
 * given" path, which php serves from the same code. Raises php's own warning
 * for a refused size, type or curve and answers 0.
 */
PH7_PRIVATE EVP_PKEY * PH7_SslGenerateKey(ph7_context *pCtx,sxi64 iBits,sxi64 iType,
	const char *zCurve,int nCurve)
{
	EVP_PKEY_CTX *pKCtx = 0;
	EVP_PKEY *pKey = 0;
	const char *zType = SslKeyTypeName(iType);
	if( zType == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unsupported private key type");
		return 0;
	}
	if( iType <= 2 && iBits < 384 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Private key length must be at least 384 bits, configured to %d",(int)iBits);
		return 0;
	}
	if( iType == 3 ){
		int nid;
		if( zCurve == 0 || nCurve < 1 ){
			/* php has no default curve: the option is REQUIRED and its absence
			 * is a configuration complaint rather than a fallback. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Missing configuration value: \"curve_name\" not set");
			return 0;
		}
		nid = OBJ_sn2nid(zCurve);
		if( nid == NID_undef ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unknown elliptic curve (short) name %.*s",nCurve,zCurve);
			return 0;
		}
		pKCtx = EVP_PKEY_CTX_new_from_name(0,"EC",0);
		if( pKCtx && EVP_PKEY_keygen_init(pKCtx) > 0
		 && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pKCtx,nid) > 0 ){
			if( EVP_PKEY_keygen(pKCtx,&pKey) <= 0 ){
				pKey = 0;
			}
		}
		if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
		return pKey;
	}
	if( iType == 1 || iType == 2 ){
		return SslGenFfcKey(zType,(int)iBits);
	}
	pKCtx = EVP_PKEY_CTX_new_from_name(0,zType,0);
	if( pKCtx && EVP_PKEY_keygen_init(pKCtx) > 0 ){
		if( iType == 0 ){
			EVP_PKEY_CTX_set_rsa_keygen_bits(pKCtx,(int)iBits);
		}
		if( EVP_PKEY_keygen(pKCtx,&pKey) <= 0 ){
			pKey = 0;
		}
	}
	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
	return pKey;
}
static int vm_builtin_openssl_pkey_new(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pOpts = 0,*pVal;
	EVP_PKEY *pKey = 0;
	sxi64 iBits = 2048,iType = 0;
	const char *zCurve = 0;
	int nCurve = 0;
	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){
		pOpts = apArg[0];
	}
	if( pOpts ){
		pKey = SslKeyFromComponents(pCtx,pOpts);
		if( pKey ){
			return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);
		}
		if( ph7_array_fetch(pOpts,"rsa",-1) || ph7_array_fetch(pOpts,"dsa",-1)
		 || ph7_array_fetch(pOpts,"dh",-1) || ph7_array_fetch(pOpts,"ec",-1) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pVal = ph7_array_fetch(pOpts,"private_key_bits",-1);
		if( pVal ){
			iBits = ph7_value_to_int64(pVal);
		}
		pVal = ph7_array_fetch(pOpts,"private_key_type",-1);
		if( pVal ){
			iType = ph7_value_to_int64(pVal);
		}
		pVal = ph7_array_fetch(pOpts,"curve_name",-1);
		if( pVal ){
			zCurve = ph7_value_to_string(pVal,&nCurve);
		}
	}
	pKey = PH7_SslGenerateKey(pCtx,iBits,iType,zCurve,nCurve);
	if( pKey == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);
}
/* ---- the two import doors ---- */
static int SslGetKeyDoor(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPublic)
{
	const char *zPass = 0;
	int nPass = 0,bOwn = 0;
	EVP_PKEY *pKey;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bPublic && nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		zPass = ph7_value_to_string(apArg[1],&nPass);
	}
	pKey = bPublic ? PH7_SslKeyOfValue(pCtx,apArg[0],1,&bOwn)
	               : SslPrivateKeyWithPass(pCtx,apArg[0],zPass,nPass,&bOwn);
	if( bOwn < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bOwn ){
		/* Already an OpenSSLAsymmetricKey: php hands back the SAME key rather
		 * than a copy, so the object identity survives the round trip. */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);
}
static int vm_builtin_openssl_pkey_get_private(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslGetKeyDoor(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_pkey_get_public(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslGetKeyDoor(pCtx,nArg,apArg,1);
}
/*
 * openssl_pkey_free(), openssl_free_key() and openssl_x509_free() are NOT
 * here, and their absence is deliberate. php 8 made all three no-ops (a key
 * and a certificate are objects now, freed with their last reference) and
 * marks each `deprecated since 8.0`; the scope policy's rule is that what php merely
 * deprecates this engine REMOVES, so a program that calls one fails loudly
 * instead of being quietly told it is doing nothing. The three names keep
 * their rows in the extension partition -- every reader filters against the
 * live VM, so they simply do not appear -- and the one call site any of the
 * four target projects reaches is recorded with the gate's baselines.
 */
/* ---- openssl_pkey_get_details() ---- */
static void SslDetailBn(ph7_context *pCtx,EVP_PKEY *pKey,ph7_value *pArray,ph7_value *pVal,
	const char *zParam,const char *zName)
{
	BIGNUM *pBn = 0;
	unsigned char *zBuf;
	int nLen;
	if( EVP_PKEY_get_bn_param(pKey,zParam,&pBn) != 1 || pBn == 0 ){
		return;
	}
	nLen = BN_num_bytes(pBn);
	zBuf = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nLen > 0 ? nLen : 1));
	if( zBuf ){
		BN_bn2bin(pBn,zBuf);
		ph7_value_string(pVal,(const char *)zBuf,nLen);
		ph7_array_add_strkey_elem(pArray,zName,pVal);
		ph7_value_reset_string_cursor(pVal);
		SyMemBackendFree(&pCtx->pVm->sAllocator,zBuf);
	}
	BN_free(pBn);
}
static int vm_builtin_openssl_pkey_get_details(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pKey;
	ph7_value *pArray,*pVal,*pSub;
	BIO *pBio;
	int iType;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = (EVP_PKEY *)PH7_SslHandleOf(apArg[0],PHL_SSL_KIND_KEY);
	if( pKey == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	pSub = ph7_context_new_array(pCtx);
	if( pArray == 0 || pVal == 0 || pSub == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_int(pVal,EVP_PKEY_get_bits(pKey));
	ph7_array_add_strkey_elem(pArray,"bits",pVal);
	/* php's `key` is the PUBLIC half in PEM, whatever kind of key this is. */
	pBio = BIO_new(BIO_s_mem());
	if( pBio ){
		char *zMem = 0;
		long nMem;
		if( PEM_write_bio_PUBKEY(pBio,pKey) == 1 ){
			nMem = BIO_get_mem_data(pBio,&zMem);
			ph7_value_string_format(pVal,"%.*s",(int)nMem,zMem);
			ph7_array_add_strkey_elem(pArray,"key",pVal);
			ph7_value_reset_string_cursor(pVal);
		}
		BIO_free(pBio);
	}
	iType = SslKeyTypeOf(pKey);
	switch( iType ){
		case 0:
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_N,"n");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_E,"e");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_D,"d");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_FACTOR1,"p");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_FACTOR2,"q");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_EXPONENT1,"dmp1");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_EXPONENT2,"dmq1");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_RSA_COEFFICIENT1,"iqmp");
			ph7_array_add_strkey_elem(pArray,"rsa",pSub);
			break;
		case 1:
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_P,"p");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_Q,"q");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_G,"g");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"priv_key");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PUB_KEY,"pub_key");
			ph7_array_add_strkey_elem(pArray,"dsa",pSub);
			break;
		case 2:
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_P,"p");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_FFC_G,"g");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"priv_key");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PUB_KEY,"pub_key");
			ph7_array_add_strkey_elem(pArray,"dh",pSub);
			break;
		case 4: case 5: case 6: case 7: {
			/* The Edwards and Montgomery keys have no numbers to report: php
			 * gives them the two RAW octet strings, under a group named after
			 * the curve itself. */
			static const char * const azRaw[] = { "x25519","ed25519","x448","ed448" };
			unsigned char aBuf[128];
			size_t nRaw = 0;
			if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PRIV_KEY,
					aBuf,sizeof(aBuf),&nRaw) == 1 ){
				ph7_value_string(pVal,(const char *)aBuf,(int)nRaw);
				ph7_array_add_strkey_elem(pSub,"priv_key",pVal);
				ph7_value_reset_string_cursor(pVal);
			}
			nRaw = 0;
			if( EVP_PKEY_get_octet_string_param(pKey,OSSL_PKEY_PARAM_PUB_KEY,
					aBuf,sizeof(aBuf),&nRaw) == 1 ){
				ph7_value_string(pVal,(const char *)aBuf,(int)nRaw);
				ph7_array_add_strkey_elem(pSub,"pub_key",pVal);
				ph7_value_reset_string_cursor(pVal);
			}
			ph7_array_add_strkey_elem(pArray,azRaw[iType - 4],pSub);
			break;
		}
		case 3: {
			char zGroup[128];
			size_t nGroup = 0;
			if( EVP_PKEY_get_utf8_string_param(pKey,OSSL_PKEY_PARAM_GROUP_NAME,
					zGroup,sizeof(zGroup),&nGroup) == 1 ){
				int nid = OBJ_sn2nid(zGroup);
				ph7_value_string(pVal,zGroup,(int)nGroup);
				ph7_array_add_strkey_elem(pSub,"curve_name",pVal);
				ph7_value_reset_string_cursor(pVal);
				if( nid != NID_undef ){
					char zOid[128];
					ASN1_OBJECT *pObj = OBJ_nid2obj(nid);
					int nOid = pObj ? OBJ_obj2txt(zOid,(int)sizeof(zOid),pObj,1) : 0;
					if( nOid > 0 ){
						ph7_value_string(pVal,zOid,nOid);
						ph7_array_add_strkey_elem(pSub,"curve_oid",pVal);
						ph7_value_reset_string_cursor(pVal);
					}
				}
			}
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_EC_PUB_X,"x");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_EC_PUB_Y,"y");
			SslDetailBn(pCtx,pKey,pSub,pVal,OSSL_PKEY_PARAM_PRIV_KEY,"d");
			ph7_array_add_strkey_elem(pArray,"ec",pSub);
			break;
		}
		default:
			break;
	}
	ph7_value_int(pVal,iType);
	ph7_array_add_strkey_elem(pArray,"type",pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* ---- openssl_pkey_export() and its file twin ---- */
static int SslPkeyExport(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)
{
	EVP_PKEY *pKey;
	BIO *pBio;
	const EVP_CIPHER *pCipher = 0;
	const char *zPass = 0,*zOut = 0;
	int nPass = 0,bOwn = 0,rc = 0,iPassArg = bToFile ? 2 : 2;
	long nOut;
	char *zMem = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[0],0,&bOwn);
	if( bOwn < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Cannot get key from parameter 1");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > iPassArg && !ph7_value_is_null(apArg[iPassArg]) ){
		zPass = ph7_value_to_string(apArg[iPassArg],&nPass);
	}
	if( zPass && nPass > 0 ){
		pCipher = EVP_aes_256_cbc();
	}
	pBio = BIO_new(BIO_s_mem());
	if( pBio == 0 ){
		SslReleaseKey(pKey,bOwn);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PEM_write_bio_PKCS8PrivateKey(pBio,pKey,pCipher,(char *)zPass,nPass,0,0) != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
		rc = -1;
	}
	if( rc == 0 ){
		nOut = BIO_get_mem_data(pBio,&zMem);
		zOut = zMem;
		if( bToFile ){
			int nPath = 0;
			const char *zPath = ph7_value_to_string(apArg[1],&nPath);
			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zOut,(sxu32)nOut);
		}else{
			ph7_value *pRes = ph7_context_new_scalar(pCtx);
			if( pRes ){
				ph7_value_string(pRes,zOut,(int)nOut);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
				ph7_context_release_value(pCtx,pRes);
			}
		}
	}
	BIO_free(pBio);
	SslReleaseKey(pKey,bOwn);
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkey_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslPkeyExport(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_pkey_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslPkeyExport(pCtx,nArg,apArg,1);
}

/* ------------------------------------------------------------------------
 * Signatures and asymmetric encryption
 * ------------------------------------------------------------------------ */
/*
 * openssl_sign() / openssl_verify(). php's `$algorithm` is the string|int the
 * digest helper reads, and `$padding` is an RSA padding mode -- 0 meaning
 * "whatever the key's default is", which for RSA is PKCS#1 v1.5.
 */
/*
 * openssl_sign() and openssl_verify() SCREEN the padding where the four raw
 * RSA doors do not: only 0 (the key's own default), PKCS#1 v1.5 and PSS are
 * accepted, and every other number -- OAEP included, which the raw doors take
 * happily -- is `Unknown padding type`. The two report that refusal
 * differently: sign answers false, verify answers -1.
 *
 * The salt length is deliberately NOT set. php leaves it at OpenSSL's own
 * default (the maximum the key allows), and pinning it to the digest's length
 * makes SHA-512 PSS impossible on a 1024-bit key -- 64 + 64 + 2 does not fit
 * in 128 bytes -- where php signs it without complaint.
 */
static int SslPaddingIsSignable(int iPadding)
{
	return iPadding == 0 || iPadding == RSA_PKCS1_PADDING || iPadding == RSA_PKCS1_PSS_PADDING;
}
static int SslApplyRsaPadding(EVP_PKEY_CTX *pKCtx,int iPadding)
{
	if( iPadding == 0 ){
		return 1;
	}
	return EVP_PKEY_CTX_set_rsa_padding(pKCtx,iPadding) > 0;
}
static int vm_builtin_openssl_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pKey;
	EVP_MD_CTX *pMdCtx = 0;
	EVP_PKEY_CTX *pKCtx = 0;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	const char *zData;
	unsigned char *zSig = 0;
	size_t nSig = 0;
	int nData = 0,bOwn = 0,iPadding = 0,rc = -1;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	if( zData == 0 ){ zData = ""; }
	if( nArg > 3 ){
		pMd = PH7_SslDigestOfValue(apArg[3],&pFetched);
	}else{
		pMd = EVP_sha1();
	}
	if( pMd == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 4 ){
		iPadding = (int)ph7_value_to_int64(apArg[4]);
	}
	if( !SslPaddingIsSignable(iPadding) ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown padding type");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwn);
	if( bOwn < 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Supplied key param cannot be coerced into a private key");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMdCtx = EVP_MD_CTX_new();
	if( pMdCtx && EVP_DigestSignInit(pMdCtx,&pKCtx,pMd,0,pKey) == 1
	 && SslApplyRsaPadding(pKCtx,iPadding)
	 && EVP_DigestSign(pMdCtx,0,&nSig,(const unsigned char *)zData,(size_t)nData) == 1 ){
		zSig = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nSig);
		if( zSig && EVP_DigestSign(pMdCtx,zSig,&nSig,(const unsigned char *)zData,(size_t)nData) == 1 ){
			ph7_value *pRes = ph7_context_new_scalar(pCtx);
			if( pRes ){
				ph7_value_string(pRes,(const char *)zSig,(int)nSig);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
				ph7_context_release_value(pCtx,pRes);
			}
			rc = 0;
		}
	}
	if( zSig ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zSig); }
	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }
	if( pFetched ){ EVP_MD_free(pFetched); }
	SslReleaseKey(pKey,bOwn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pKey;
	EVP_MD_CTX *pMdCtx = 0;
	EVP_PKEY_CTX *pKCtx = 0;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	const char *zData,*zSig;
	int nData = 0,nSig = 0,bOwn = 0,iPadding = 0,iRc;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	zSig = ph7_value_to_string(apArg[1],&nSig);
	if( zData == 0 ){ zData = ""; }
	if( zSig == 0 ){ zSig = ""; }
	if( nArg > 3 ){
		pMd = PH7_SslDigestOfValue(apArg[3],&pFetched);
	}else{
		pMd = EVP_sha1();
	}
	if( pMd == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 4 ){
		iPadding = (int)ph7_value_to_int64(apArg[4]);
	}
	if( !SslPaddingIsSignable(iPadding) ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown padding type");
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],1,&bOwn);
	if( bOwn < 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Supplied key param cannot be coerced into a public key");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMdCtx = EVP_MD_CTX_new();
	iRc = -1;
	if( pMdCtx && EVP_DigestVerifyInit(pMdCtx,&pKCtx,pMd,0,pKey) == 1
	 && SslApplyRsaPadding(pKCtx,iPadding) ){
		iRc = EVP_DigestVerify(pMdCtx,(const unsigned char *)zSig,(size_t)nSig,
			(const unsigned char *)zData,(size_t)nData);
	}
	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }
	if( pFetched ){ EVP_MD_free(pFetched); }
	SslReleaseKey(pKey,bOwn);
	if( iRc < 0 ){
		/* php's -1 is a FAILED verification rather than an error: the three
		 * answers are 1, 0 and -1, and only a bad key or digest is `false`. */
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( iRc != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_int(pCtx,iRc == 1 ? 1 : 0);
	return PH7_OK;
}
/*
 * The four raw RSA doors. `$padding` defaults to PKCS#1 v1.5 in all four, and
 * the two DECRYPT directions size their output from the key rather than from
 * the input -- a plaintext shorter than the modulus comes back at its own
 * length, which is what the padding removed.
 */
static int SslRsaDoor(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPublicKey,int bEncrypt,
	const char *zBadKey)
{
	EVP_PKEY *pKey;
	EVP_PKEY_CTX *pKCtx = 0;
	const char *zIn;
	unsigned char *zOut = 0;
	size_t nOut = 0;
	int nIn = 0,bOwn = 0,iPadding = RSA_PKCS1_PADDING,rc = -1;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nIn);
	if( zIn == 0 ){ zIn = ""; }
	if( nArg > 3 ){
		iPadding = (int)ph7_value_to_int64(apArg[3]);
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],bPublicKey,&bOwn);
	if( bOwn < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBadKey);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKCtx = EVP_PKEY_CTX_new(pKey,0);
	if( pKCtx ){
		int (*xInit)(EVP_PKEY_CTX *);
		int (*xRun)(EVP_PKEY_CTX *,unsigned char *,size_t *,const unsigned char *,size_t);
		if( bEncrypt ){
			xInit = bPublicKey ? EVP_PKEY_encrypt_init : EVP_PKEY_sign_init;
			xRun  = bPublicKey ? EVP_PKEY_encrypt : 0;
		}else{
			xInit = bPublicKey ? EVP_PKEY_verify_recover_init : EVP_PKEY_decrypt_init;
			xRun  = bPublicKey ? EVP_PKEY_verify_recover : EVP_PKEY_decrypt;
		}
		if( bEncrypt && !bPublicKey ){
			/* A PRIVATE encrypt is a raw signature: OpenSSL spells it
			 * sign_init/sign with no digest, which is what php's
			 * openssl_private_encrypt() produces and what
			 * openssl_public_decrypt() recovers. */
			xRun = EVP_PKEY_sign;
		}
		if( xInit(pKCtx) > 0
		 && EVP_PKEY_CTX_set_rsa_padding(pKCtx,iPadding) > 0
		 && xRun(pKCtx,0,&nOut,(const unsigned char *)zIn,(size_t)nIn) > 0 ){
			zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
				(sxu32)(nOut > 0 ? nOut : 1));
			if( zOut && xRun(pKCtx,zOut,&nOut,(const unsigned char *)zIn,(size_t)nIn) > 0 ){
				ph7_value *pRes = ph7_context_new_scalar(pCtx);
				if( pRes ){
					ph7_value_string(pRes,(const char *)zOut,(int)nOut);
					PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
					ph7_context_release_value(pCtx,pRes);
				}
				rc = 0;
			}
		}
	}
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
	SslReleaseKey(pKey,bOwn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_public_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslRsaDoor(pCtx,nArg,apArg,1,1,"key parameter is not a valid public key");
}
static int vm_builtin_openssl_private_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslRsaDoor(pCtx,nArg,apArg,0,0,"key parameter is not a valid private key");
}
static int vm_builtin_openssl_private_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslRsaDoor(pCtx,nArg,apArg,0,1,"key parameter is not a valid private key");
}
static int vm_builtin_openssl_public_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslRsaDoor(pCtx,nArg,apArg,1,0,"key parameter is not a valid public key");
}
/*
 * openssl_seal() / openssl_open(): a symmetric key per message, encrypted once
 * per recipient. The sealed LENGTH is what seal() answers, not a boolean.
 */
static int vm_builtin_openssl_seal(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const EVP_CIPHER *pCipher;
	EVP_CIPHER *pFetched = 0;
	EVP_CIPHER_CTX *pEvp = 0;
	EVP_PKEY **apKey = 0;
	unsigned char **apEk = 0;
	int *aEkLen = 0,*aOwn = 0;
	unsigned char *zIv = 0,*zOut = 0;
	const char *zData,*zMethod;
	ph7_value *pKeys,*pVal = 0,*pEkArray = 0;
	int nData = 0,nMethod = 0,nKey = 0,i,nOut = 0,nFinal = 0,nIv,rc = -1,bShape = 0;
	if( nArg < 5 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	if( zData == 0 ){ zData = ""; }
	pKeys = apArg[3];
	if( !ph7_value_is_array(pKeys) || (nKey = (int)ph7_array_count(pKeys)) < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #4 ($public_key) must not be empty",ph7_function_name(pCtx));
	}
	zMethod = ph7_value_to_string(apArg[4],&nMethod);
	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Zeroed AS THEY ARE TAKEN, not after the four succeed: the cleanup at
	 * `done:` walks apKey[] and aOwn[], and a partial allocation failure would
	 * otherwise send it through uninitialized pointers. */
	apKey = (EVP_PKEY **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(void *) * nKey));
	if( apKey ){ SyZero(apKey,(sxu32)(sizeof(void *) * nKey)); }
	apEk = (unsigned char **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(void *) * nKey));
	if( apEk ){ SyZero(apEk,(sxu32)(sizeof(void *) * nKey)); }
	aEkLen = (int *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(int) * nKey));
	if( aEkLen ){ SyZero(aEkLen,(sxu32)(sizeof(int) * nKey)); }
	aOwn = (int *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(sizeof(int) * nKey));
	if( aOwn ){ SyZero(aOwn,(sxu32)(sizeof(int) * nKey)); }
	if( apKey == 0 || apEk == 0 || aEkLen == 0 || aOwn == 0 ){
		goto done;
	}
	for( i = 0 ; i < nKey ; ++i ){
		ph7_value *pEntry = SslArrayNth(pKeys,i);
		apKey[i] = pEntry ? PH7_SslKeyOfValue(pCtx,pEntry,1,&aOwn[i]) : 0;
		if( aOwn[i] < 0 ){
			bShape = 1;
			goto done;
		}
		if( apKey[i] == 0 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Not a public key (%dth member of pubkeys)",i + 1);
			goto done;
		}
		apEk[i] = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
			(sxu32)EVP_PKEY_get_size(apKey[i]));
		if( apEk[i] == 0 ){
			goto done;
		}
	}
	nIv = EVP_CIPHER_get_iv_length(pCipher);
	if( nIv > 0 ){
		zIv = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nIv);
		if( zIv == 0 ){
			goto done;
		}
	}
	pEvp = EVP_CIPHER_CTX_new();
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));
	if( pEvp == 0 || zOut == 0 ){
		goto done;
	}
	if( EVP_SealInit(pEvp,pCipher,apEk,aEkLen,zIv,apKey,nKey) <= 0
	 || EVP_SealUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1
	 || EVP_SealFinal(pEvp,zOut + nOut,&nFinal) != 1 ){
		goto done;
	}
	nOut += nFinal;
	pVal = ph7_context_new_scalar(pCtx);
	pEkArray = ph7_context_new_array(pCtx);
	if( pVal == 0 || pEkArray == 0 ){
		goto done;
	}
	ph7_value_string(pVal,(const char *)zOut,nOut);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pVal);
	ph7_value_reset_string_cursor(pVal);
	for( i = 0 ; i < nKey ; ++i ){
		ph7_value_string(pVal,(const char *)apEk[i],aEkLen[i]);
		ph7_array_add_elem(pEkArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pEkArray);
	if( nArg > 5 ){
		ph7_value_string(pVal,(const char *)(zIv ? zIv : (unsigned char *)""),nIv > 0 ? nIv : 0);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[5],pVal);
		ph7_value_reset_string_cursor(pVal);
	}
	rc = 0;
done:
	if( apKey ){
		for( i = 0 ; i < nKey ; ++i ){
			SslReleaseKey(apKey[i],aOwn ? aOwn[i] : 0);
			if( apEk && apEk[i] ){ SyMemBackendFree(&pCtx->pVm->sAllocator,apEk[i]); }
		}
		SyMemBackendFree(&pCtx->pVm->sAllocator,apKey);
	}
	if( apEk ){ SyMemBackendFree(&pCtx->pVm->sAllocator,apEk); }
	if( aEkLen ){ SyMemBackendFree(&pCtx->pVm->sAllocator,aEkLen); }
	if( aOwn ){ SyMemBackendFree(&pCtx->pVm->sAllocator,aOwn); }
	if( zIv ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zIv); }
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }
	if( pFetched ){ EVP_CIPHER_free(pFetched); }
	if( bShape ){
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int(pCtx,nOut);
	}
	return PH7_OK;
}
static int vm_builtin_openssl_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const EVP_CIPHER *pCipher;
	EVP_CIPHER *pFetched = 0;
	EVP_CIPHER_CTX *pEvp = 0;
	EVP_PKEY *pKey;
	const char *zData,*zEk,*zMethod,*zIv = 0;
	unsigned char *zOut = 0;
	int nData = 0,nEk = 0,nMethod = 0,nIv = 0,bOwn = 0,nOut = 0,nFinal = 0,rc = -1;
	if( nArg < 5 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[0],&nData);
	zEk = ph7_value_to_string(apArg[2],&nEk);
	zMethod = ph7_value_to_string(apArg[4],&nMethod);
	if( zData == 0 ){ zData = ""; }
	if( zEk == 0 ){ zEk = ""; }
	if( nArg > 5 && !ph7_value_is_null(apArg[5]) ){
		zIv = ph7_value_to_string(apArg[5],&nIv);
	}
	pCipher = nMethod > 0 ? SslCipherByName(zMethod,&pFetched) : 0;
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown cipher algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwn);
	if( bOwn < 0 ){
		if( pFetched ){ EVP_CIPHER_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to coerce parameter 4 into a private key");
		if( pFetched ){ EVP_CIPHER_free(pFetched); }
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pEvp = EVP_CIPHER_CTX_new();
	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
		(sxu32)(nData + EVP_CIPHER_get_block_size(pCipher) + 1));
	if( pEvp == 0 || zOut == 0 ){
		goto done;
	}
	if( EVP_OpenInit(pEvp,pCipher,(const unsigned char *)zEk,nEk,
			(const unsigned char *)zIv,pKey) <= 0
	 || EVP_OpenUpdate(pEvp,zOut,&nOut,(const unsigned char *)zData,nData) != 1
	 || EVP_OpenFinal(pEvp,zOut + nOut,&nFinal) != 1 ){
		goto done;
	}
	nOut += nFinal;
	{
		ph7_value *pRes = ph7_context_new_scalar(pCtx);
		if( pRes ){
			ph7_value_string(pRes,(const char *)zOut,nOut);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
			ph7_context_release_value(pCtx,pRes);
		}
	}
	rc = 0;
done:
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( pEvp ){ EVP_CIPHER_CTX_free(pEvp); }
	if( pFetched ){ EVP_CIPHER_free(pFetched); }
	SslReleaseKey(pKey,bOwn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
/*
 * openssl_pkey_derive(): one side's public key and the other's private one.
 *
 * php's THIRD parameter is not here. It deprecates the parameter itself rather
 * than any particular value -- an explicit 0 raises "the $key_length parameter
 * is deprecated as it is either ignored or truncates the key" just as a 16
 * does -- because the exchange produces what it produces and asking for fewer
 * bytes hands back a PREFIX that is not a shorter shared secret. The scope policy refuses
 * the spelling, so the signature declares two parameters and a third argument
 * is `expects exactly 2 arguments, 3 given`.
 */
static int vm_builtin_openssl_pkey_derive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pPub,*pPriv;
	EVP_PKEY_CTX *pKCtx = 0;
	unsigned char *zOut = 0;
	size_t nOut = 0;
	int bOwnPub = 0,bOwnPriv = 0,rc = -1;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pPub = PH7_SslKeyOfValue(pCtx,apArg[0],1,&bOwnPub);
	pPriv = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnPriv);
	if( bOwnPub < 0 || bOwnPriv < 0 ){
		SslReleaseKey(pPub,bOwnPub);
		SslReleaseKey(pPriv,bOwnPriv);
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pPub == 0 || pPriv == 0 ){
		SslReleaseKey(pPub,bOwnPub);
		SslReleaseKey(pPriv,bOwnPriv);
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKCtx = EVP_PKEY_CTX_new(pPriv,0);
	if( pKCtx && EVP_PKEY_derive_init(pKCtx) > 0
	 && EVP_PKEY_derive_set_peer(pKCtx,pPub) > 0
	 && EVP_PKEY_derive(pKCtx,0,&nOut) > 0 ){
		zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nOut > 0 ? nOut : 1));
		if( zOut && EVP_PKEY_derive(pKCtx,zOut,&nOut) > 0 ){
			ph7_result_string(pCtx,(const char *)zOut,(int)nOut);
			rc = 0;
		}
	}
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
	SslReleaseKey(pPub,bOwnPub);
	SslReleaseKey(pPriv,bOwnPriv);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * openssl_dh_compute_key(): the DH-only spelling of the same exchange, whose
 * public side is the RAW public value rather than an encoded key.
 */
static int vm_builtin_openssl_dh_compute_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pPriv,*pPeer = 0;
	EVP_PKEY_CTX *pKCtx = 0;
	OSSL_PARAM_BLD *pBld = 0;
	OSSL_PARAM *pParams = 0;
	BIGNUM *pPub = 0,*pP = 0,*pG = 0;
	const char *zPub;
	unsigned char *zOut = 0;
	size_t nOut = 0;
	int nPub = 0,rc = -1;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPub = ph7_value_to_string(apArg[0],&nPub);
	pPriv = (EVP_PKEY *)PH7_SslHandleOf(apArg[1],PHL_SSL_KIND_KEY);
	if( pPriv == 0 || zPub == 0 || nPub < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( EVP_PKEY_get_bn_param(pPriv,OSSL_PKEY_PARAM_FFC_P,&pP) != 1
	 || EVP_PKEY_get_bn_param(pPriv,OSSL_PKEY_PARAM_FFC_G,&pG) != 1 ){
		goto done;
	}
	pPub = BN_bin2bn((const unsigned char *)zPub,nPub,0);
	pBld = OSSL_PARAM_BLD_new();
	if( pPub == 0 || pBld == 0 ){
		goto done;
	}
	if( !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_FFC_P,pP)
	 || !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_FFC_G,pG)
	 || !OSSL_PARAM_BLD_push_BN(pBld,OSSL_PKEY_PARAM_PUB_KEY,pPub) ){
		goto done;
	}
	pParams = OSSL_PARAM_BLD_to_param(pBld);
	if( pParams == 0 ){
		goto done;
	}
	pKCtx = EVP_PKEY_CTX_new_from_name(0,"DH",0);
	if( pKCtx == 0 || EVP_PKEY_fromdata_init(pKCtx) <= 0
	 || EVP_PKEY_fromdata(pKCtx,&pPeer,EVP_PKEY_PUBLIC_KEY,pParams) <= 0 ){
		goto done;
	}
	EVP_PKEY_CTX_free(pKCtx);
	pKCtx = EVP_PKEY_CTX_new(pPriv,0);
	if( pKCtx && EVP_PKEY_derive_init(pKCtx) > 0
	 && EVP_PKEY_derive_set_peer(pKCtx,pPeer) > 0
	 && EVP_PKEY_derive(pKCtx,0,&nOut) > 0 ){
		zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nOut > 0 ? nOut : 1));
		if( zOut && EVP_PKEY_derive(pKCtx,zOut,&nOut) > 0 ){
			ph7_result_string(pCtx,(const char *)zOut,(int)nOut);
			rc = 0;
		}
	}
done:
	if( zOut ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zOut); }
	if( pKCtx ){ EVP_PKEY_CTX_free(pKCtx); }
	if( pPeer ){ EVP_PKEY_free(pPeer); }
	if( pParams ){ OSSL_PARAM_free(pParams); }
	if( pBld ){ OSSL_PARAM_BLD_free(pBld); }
	if( pPub ){ BN_free(pPub); }
	if( pP ){ BN_free(pP); }
	if( pG ){ BN_free(pG); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * The SPKAC family. php's own asymmetry lives here and is reproduced rather
 * than smoothed: openssl_spki_new() answers a string PREFIXED with `SPKAC=`,
 * and the three readers take the bare base64 -- so handing new()'s answer
 * straight back to verify() is `Unable to decode supplied SPKAC` under php too.
 */
static NETSCAPE_SPKI * SslSpkiOfArg(ph7_context *pCtx,ph7_value *pVal)
{
	const char *zStr;
	int nStr = 0;
	SXUNUSED(pCtx);
	zStr = ph7_value_to_string(pVal,&nStr);
	if( zStr == 0 || nStr < 1 ){
		return 0;
	}
	return NETSCAPE_SPKI_b64_decode(zStr,nStr);
}
static int vm_builtin_openssl_spki_new(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	EVP_PKEY *pKey;
	NETSCAPE_SPKI *pSpki;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	const char *zChallenge;
	char *zOut;
	int nChallenge = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = (EVP_PKEY *)PH7_SslHandleOf(apArg[0],PHL_SSL_KIND_KEY);
	zChallenge = ph7_value_to_string(apArg[1],&nChallenge);
	if( pKey == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		pMd = PH7_SslDigestOfValue(apArg[2],&pFetched);
	}else{
		pMd = EVP_md5();
	}
	if( pMd == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSpki = NETSCAPE_SPKI_new();
	if( pSpki == 0 ){
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( (nChallenge > 0 && ASN1_STRING_set(pSpki->spkac->challenge,zChallenge,nChallenge) != 1)
	 || NETSCAPE_SPKI_set_pubkey(pSpki,pKey) != 1
	 || NETSCAPE_SPKI_sign(pSpki,pKey,pMd) <= 0 ){
		NETSCAPE_SPKI_free(pSpki);
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zOut = NETSCAPE_SPKI_b64_encode(pSpki);
	NETSCAPE_SPKI_free(pSpki);
	if( pFetched ){ EVP_MD_free(pFetched); }
	if( zOut == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string_format(pCtx,"SPKAC=%s",zOut);
	OPENSSL_free(zOut);
	return PH7_OK;
}
static int vm_builtin_openssl_spki_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	NETSCAPE_SPKI *pSpki;
	EVP_PKEY *pKey;
	int iRc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSpki = SslSpkiOfArg(pCtx,apArg[0]);
	if( pSpki == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode supplied SPKAC");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = NETSCAPE_SPKI_get_pubkey(pSpki);
	iRc = pKey ? NETSCAPE_SPKI_verify(pSpki,pKey) : -1;
	if( pKey ){ EVP_PKEY_free(pKey); }
	NETSCAPE_SPKI_free(pSpki);
	if( iRc != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,iRc == 1);
	return PH7_OK;
}
static int vm_builtin_openssl_spki_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	NETSCAPE_SPKI *pSpki;
	EVP_PKEY *pKey;
	BIO *pBio;
	char *zMem = 0;
	long nMem;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSpki = SslSpkiOfArg(pCtx,apArg[0]);
	if( pSpki == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode supplied SPKAC");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = NETSCAPE_SPKI_get_pubkey(pSpki);
	NETSCAPE_SPKI_free(pSpki);
	pBio = pKey ? BIO_new(BIO_s_mem()) : 0;
	if( pBio == 0 || PEM_write_bio_PUBKEY(pBio,pKey) != 1 ){
		if( pBio ){ BIO_free(pBio); }
		if( pKey ){ EVP_PKEY_free(pKey); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMem = BIO_get_mem_data(pBio,&zMem);
	ph7_result_string(pCtx,zMem,(int)nMem);
	BIO_free(pBio);
	EVP_PKEY_free(pKey);
	return PH7_OK;
}
static int vm_builtin_openssl_spki_export_challenge(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	NETSCAPE_SPKI *pSpki;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSpki = SslSpkiOfArg(pCtx,apArg[0]);
	if( pSpki == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to decode SPKAC");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)ASN1_STRING_get0_data(pSpki->spkac->challenge),
		ASN1_STRING_length(pSpki->spkac->challenge));
	NETSCAPE_SPKI_free(pSpki);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Installation
 * ------------------------------------------------------------------------ */
/*
 * The three handle classes. php's OpenSSLCertificate,
 * OpenSSLCertificateSigningRequest and OpenSSLAsymmetricKey each declare no
 * method, no constant and no property, print as an empty object on every
 * presentation surface, refuse `new`, refuse `clone` and refuse serialization
 * -- so the one slot here is engine storage, hidden so it appears on none of
 * them. `(int)` on one is the object handle, silently, which is
 * PH7_CLASS_HANDLE_ID.
 */
PH7_PRIVATE sxi32 PH7_VmInstallOpenSsl(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ PHL_SSL_CLASS_CERT, 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE
		  |PH7_CLASS_NOSERIALIZE|PH7_CLASS_HANDLE_ID,
		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 },
		{ PHL_SSL_CLASS_CSR, 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE
		  |PH7_CLASS_NOSERIALIZE|PH7_CLASS_HANDLE_ID,
		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 },
		{ PHL_SSL_CLASS_KEY, 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE
		  |PH7_CLASS_NOSERIALIZE|PH7_CLASS_HANDLE_ID,
		  0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), SslInstanceRelease, 0, 0 }
	};
	static const struct { const char *zName; const char *zRefusal; } aRefusal[] = {
		{ PHL_SSL_CLASS_CERT,
		  "Cannot directly construct OpenSSLCertificate, use openssl_x509_read() instead" },
		{ PHL_SSL_CLASS_CSR,
		  "Cannot directly construct OpenSSLCertificateSigningRequest, use openssl_csr_new() instead" },
		{ PHL_SSL_CLASS_KEY,
		  "Cannot directly construct OpenSSLAsymmetricKey, use openssl_pkey_new() instead" }
	};
	sxi32 rc;
	sxu32 n;
	pVm->pSslObjs = 0;
	pVm->pSslErrors = 0;
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aRefusal) ; ++n ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),aRefusal[n].zName,
			(sxu32)SyStrlen(aRefusal[n].zName),FALSE,0);
		if( pClass ){
			pClass->zNewRefusal = aRefusal[n].zRefusal;
			pClass->xCmp = PH7_NativeCmpOpaqueHandle;
		}
	}
	return PH7_VmInstallOpenSslX509(&(*pVm));
}
/* The functions this unit owns, in php's own registration order. The
 * certificate half registers its own (vm_openssl_x509.c). */
PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslFuncTable(sxu32 *pnEntry)
{
	static const ph7_builtin_func aFunc[] = {
		{ "openssl_pbkdf2",               vm_builtin_openssl_pbkdf2               },
		{ "openssl_error_string",         vm_builtin_openssl_error_string         },
		{ "openssl_get_md_methods",       vm_builtin_openssl_get_md_methods       },
		{ "openssl_get_cipher_methods",   vm_builtin_openssl_get_cipher_methods   },
		{ "openssl_get_curve_names",      vm_builtin_openssl_get_curve_names      },
		{ "openssl_digest",               vm_builtin_openssl_digest               },
		{ "openssl_encrypt",              vm_builtin_openssl_encrypt              },
		{ "openssl_decrypt",              vm_builtin_openssl_decrypt              },
		{ "openssl_cipher_iv_length",     vm_builtin_openssl_cipher_iv_length     },
		{ "openssl_cipher_key_length",    vm_builtin_openssl_cipher_key_length    },
		{ "openssl_random_pseudo_bytes",  vm_builtin_openssl_random_pseudo_bytes  },
		{ "openssl_get_cert_locations",   vm_builtin_openssl_get_cert_locations   },
		{ "openssl_pkey_new",             vm_builtin_openssl_pkey_new             },
		{ "openssl_pkey_export_to_file",  vm_builtin_openssl_pkey_export_to_file  },
		{ "openssl_pkey_export",          vm_builtin_openssl_pkey_export          },
		{ "openssl_pkey_get_public",      vm_builtin_openssl_pkey_get_public      },
		{ "openssl_get_publickey",        vm_builtin_openssl_pkey_get_public      },
		{ "openssl_pkey_get_private",     vm_builtin_openssl_pkey_get_private     },
		{ "openssl_get_privatekey",       vm_builtin_openssl_pkey_get_private     },
		{ "openssl_pkey_get_details",     vm_builtin_openssl_pkey_get_details     },
		{ "openssl_private_encrypt",      vm_builtin_openssl_private_encrypt      },
		{ "openssl_private_decrypt",      vm_builtin_openssl_private_decrypt      },
		{ "openssl_public_encrypt",       vm_builtin_openssl_public_encrypt       },
		{ "openssl_public_decrypt",       vm_builtin_openssl_public_decrypt       },
		{ "openssl_sign",                 vm_builtin_openssl_sign                 },
		{ "openssl_verify",               vm_builtin_openssl_verify               },
		{ "openssl_seal",                 vm_builtin_openssl_seal                 },
		{ "openssl_open",                 vm_builtin_openssl_open                 },
		{ "openssl_dh_compute_key",       vm_builtin_openssl_dh_compute_key       },
		{ "openssl_pkey_derive",          vm_builtin_openssl_pkey_derive          },
		{ "openssl_spki_new",             vm_builtin_openssl_spki_new             },
		{ "openssl_spki_verify",          vm_builtin_openssl_spki_verify          },
		{ "openssl_spki_export",          vm_builtin_openssl_spki_export          },
		{ "openssl_spki_export_challenge",vm_builtin_openssl_spki_export_challenge}
	};
	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);
	return aFunc;
}

#else
/* Ensure non-empty translation unit when openssl is disabled (MSVC C4206) */
typedef int vm_openssl_unused;
#endif /* PH7_ENABLE_OPENSSL */
