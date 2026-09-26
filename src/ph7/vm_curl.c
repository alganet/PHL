/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_CURL
#include "curl_int.h"

/*
 * Section:
 *    ext/curl -- php's binding of libcurl.
 * Status:
 *    In progress. This unit currently carries the library-wide surface:
 *    curl_version() and the three strerror() families. The handle classes,
 *    the option table and the transfer verbs follow.
 *
 * WHY A BINDING AND NOT A REIMPLEMENTATION. php's ext/curl is a thin shell
 * over libcurl, so the contract a program depends on is the LIBRARY's, not
 * the extension's -- the same relationship ext/pdo_sqlite has with
 * libsqlite3. Every answer here is therefore derived by asking php 8.5 and
 * libcurl 8.5 the same question, never by reading php's source: see the
 * per-function notes below for the ones a careful reading would have got
 * wrong.
 *
 * MEMORY MODEL. libcurl stays on its own (system) allocator, like libxml2 and
 * sqlite3 before it. Routing it through SyMemBackend would subject library
 * internals to the PHL_MAX_ALLOC fault injection the stress tier uses, and
 * libcurl does not tolerate mid-transfer OOM injection the way engine code
 * does.
 */

/*
 * curl_global_init() once per process. libcurl's own documentation calls this
 * not thread-safe, so it must not be left to the first curl_easy_init() on
 * whichever thread gets there first (the -S server pre-forks, and
 * PH7_ENABLE_THREADS builds share the process).
 */
PH7_PRIVATE void PH7_CurlGlobalInit(void)
{
	static int bInit = 0;
	if( !bInit ){
		curl_global_init(CURL_GLOBAL_DEFAULT);
		bInit = 1;
	}
}

/* ------------------------------------------------------------------------
 * Handle lifetime
 * ------------------------------------------------------------------------ */
/*
 * A handle is reached from its CurlHandle object through the hidden `__res`
 * slot and is ALSO chained on the per-VM registry, because a PH7 resource
 * carries no destructor: the sweep at VM reset/release is what closes a
 * transfer a script left open. Unlike ext/pdo's connections, a CurlHandle IS
 * cloneable (php maps clone to curl_easy_duphandle), so the clone gets its own
 * record and its own libcurl handle -- never a second object over one CURL*.
 */
static void CurlBlankSlot(ph7_class_instance *pOwner);

static phl_curl * CurlNewHandle(ph7_vm *pVm)
{
	phl_curl *pCurl = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));
	if( pCurl == 0 ){
		return 0;
	}
	SyZero(pCurl,sizeof(phl_curl));
	pCurl->pVm = pVm;
	pCurl->pEasy = curl_easy_init();
	if( pCurl->pEasy == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pCurl);
		return 0;
	}
	/*
	 * php gives every handle its own error buffer at creation, and that is
	 * what curl_error() reports -- not curl_easy_strerror() of the code. It
	 * has to be re-applied after curl_easy_reset(), which clears it.
	 */
	curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);
	pCurl->pNext = (phl_curl *)pVm->pCurlHandles;
	pVm->pCurlHandles = pCurl;
	return pCurl;
}
/*
 * The slot a handle keeps one option's curl_slist in, created on first use.
 * Keyed by option, so setting CURLOPT_HTTPHEADER twice replaces one list
 * rather than accumulating two.
 */
static phl_curl_slist * CurlSlistSlot(phl_curl *pCurl,sxi64 iOpt)
{
	phl_curl_slist *pSlot = pCurl->pSlists;
	while( pSlot ){
		if( pSlot->iOpt == iOpt ){
			return pSlot;
		}
		pSlot = pSlot->pNext;
	}
	pSlot = (phl_curl_slist *)SyMemBackendAlloc(&pCurl->pVm->sAllocator,sizeof(phl_curl_slist));
	if( pSlot == 0 ){
		return 0;
	}
	SyZero(pSlot,sizeof(phl_curl_slist));
	pSlot->iOpt = iOpt;
	pSlot->pNext = pCurl->pSlists;
	pCurl->pSlists = pSlot;
	return pSlot;
}
static void CurlFreeSlists(phl_curl *pCurl)
{
	phl_curl_slist *pSlot = pCurl->pSlists;
	while( pSlot ){
		phl_curl_slist *pNext = pSlot->pNext;
		if( pSlot->pList ){
			curl_slist_free_all(pSlot->pList);
		}
		SyMemBackendFree(&pCurl->pVm->sAllocator,pSlot);
		pSlot = pNext;
	}
	pCurl->pSlists = 0;
}
static void CurlFreeHandle(phl_curl *pCurl)
{
	if( pCurl->pEasy ){
		/* The handle goes first: libcurl reads the lists during a transfer and
		 * must not be left pointing at freed memory even for an instant. */
		curl_easy_cleanup(pCurl->pEasy);
		pCurl->pEasy = 0;
	}
	CurlFreeSlists(pCurl);
}
/*
 * Free every registered handle. Called from PH7_CurlVmReset (a reused VM --
 * the -S server's -- must not answer the next request through a connection the
 * previous one opened) and from PH7_CurlVmRelease before the allocator holding
 * the shells is torn down.
 */
static void CurlVmSweep(ph7_vm *pVm)
{
	phl_curl *pCurl = (phl_curl *)pVm->pCurlHandles;
	while( pCurl ){
		phl_curl *pNext = pCurl->pNext;
		CurlBlankSlot(pCurl->pOwner);
		CurlFreeHandle(pCurl);
		SyMemBackendFree(&pVm->sAllocator,pCurl);
		pCurl = pNext;
	}
	pVm->pCurlHandles = 0;
}
PH7_PRIVATE void PH7_CurlVmReset(ph7_vm *pVm)
{
	CurlVmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_CurlVmRelease(ph7_vm *pVm)
{
	CurlVmSweep(&(*pVm));
}
/*
 * Blank the hidden slot of the object whose record we are about to free, so
 * the object cannot outlive its record and then read freed memory to ask
 * whether it still owns one.
 */
static void CurlBlankSlot(ph7_class_instance *pOwner)
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
/* The handle behind a `__res` slot value. */
static phl_curl * CurlOfValue(ph7_value *pVal)
{
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return (phl_curl *)pVal->x.pOther;
}
static phl_curl * CurlOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);
	return CurlOfValue(PH7_ClassInstanceFetchAttr(pThis,&sAttr));
}
/* Store one handle in the receiver's hidden slot. */
static int CurlAttach(ph7_class_instance *pThis,phl_curl *pCurl)
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
	pRes->x.pOther = pCurl;
	MemObjSetType(pRes,MEMOBJ_RES);
	pCurl->pOwner = pThis;
	return 0;
}
/*
 * The object is going away: close its transfer now rather than at VM reset, so
 * a script that drops its last reference releases the socket there. The shell
 * stays on the registry (the sweep frees it) because the slot is still
 * reachable while the instance is being torn down.
 */
static void CurlInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_curl *pCurl = CurlOfInstance(pThis);
	SXUNUSED(pVm);
	if( pCurl == 0 || pCurl->pOwner != pThis ){
		return;
	}
	CurlFreeHandle(pCurl);
	pCurl->pOwner = 0;
}
/*
 * php's clone_obj for CurlHandle: curl_easy_duphandle(). Every OPTION comes
 * across and nothing else does -- the copy starts with a clean error state
 * even when the source's last transfer failed.
 *
 * The error buffer is why this needs care rather than a bare duphandle.
 * CURLOPT_ERRORBUFFER is an option like any other, so libcurl copies its
 * VALUE: the clone would point at the SOURCE's buffer, write its own failures
 * into it (php's answer for the source would change when the clone failed) and
 * keep writing there after the source was freed. Re-pointing it at the clone's
 * own storage is what php does and what makes the two independent.
 *
 * Runs after the slot-by-slot copy, so the clone's `__res` currently holds the
 * SOURCE's record: every exit here has to overwrite or blank it, or two
 * instances would free one CURL*.
 */
static void CurlInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)
{
	phl_curl *pFrom = CurlOfInstance(pSrc);
	phl_curl *pNew;
	CURL *pDup;
	if( pFrom == 0 || pFrom->pEasy == 0 ){
		CurlBlankSlot(pClone);
		return;
	}
	pNew = (phl_curl *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_curl));
	if( pNew == 0 ){
		CurlBlankSlot(pClone);
		return;
	}
	SyZero(pNew,sizeof(phl_curl));
	pNew->pVm = pVm;
	pDup = curl_easy_duphandle(pFrom->pEasy);
	if( pDup == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pNew);
		CurlBlankSlot(pClone);
		return;
	}
	pNew->pEasy = pDup;
	curl_easy_setopt(pDup,CURLOPT_ERRORBUFFER,pNew->zErrBuf);
	/*
	 * The slists have to be rebuilt for the copy, and pointed at from the copy,
	 * for the same reason the source owns them: libcurl stores the POINTER.
	 * Left alone, the copy would read the source's lists and keep reading them
	 * after the source freed them. (php duplicates them here too, which is the
	 * evidence that duphandle does not.)
	 */
	{
		phl_curl_slist *pFromSlot = pFrom->pSlists;
		while( pFromSlot ){
			struct curl_slist *pCopy = 0;
			struct curl_slist *pWalk = pFromSlot->pList;
			int bOk = 1;
			while( pWalk ){
				struct curl_slist *pNextNode = curl_slist_append(pCopy,pWalk->data);
				if( pNextNode == 0 ){ bOk = 0; break; }
				pCopy = pNextNode;
				pWalk = pWalk->next;
			}
			if( bOk ){
				phl_curl_slist *pSlot = CurlSlistSlot(pNew,pFromSlot->iOpt);
				if( pSlot ){
					pSlot->pList = pCopy;
					curl_easy_setopt(pDup,(CURLoption)pFromSlot->iOpt,pCopy);
				}else if( pCopy ){
					curl_slist_free_all(pCopy);
				}
			}else if( pCopy ){
				curl_slist_free_all(pCopy);
			}
			pFromSlot = pFromSlot->pNext;
		}
	}
	pNew->pNext = (phl_curl *)pVm->pCurlHandles;
	pVm->pCurlHandles = pNew;
	if( CurlAttach(pClone,pNew) != 0 ){
		CurlBlankSlot(pClone);
	}
}

/*
 * The CurlHandle argument of every verb. The signature table has already
 * screened the TYPE (php's "must be of type CurlHandle, null given" comes from
 * there), so a miss here means the object is one the engine tore down -- which
 * php cannot produce and which must not be a crash.
 */
static phl_curl * CurlArg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	SXUNUSED(nArg);
	pThis = (apArg && ph7_value_is_object(apArg[0])) ?
		(ph7_class_instance *)apArg[0]->x.pOther : 0;
	if( pThis == 0 ){
		return 0;
	}
	SXUNUSED(pCtx);
	return CurlOfInstance(pThis);
}

/* ===== Constants ===== */

/*
 * php's whole ext/curl constant surface: 679 names, dumped from php 8.5.9's
 * own get_defined_constants(true)['curl'] and kept in php's REGISTRATION
 * order (which is what get_defined_constants reports).
 *
 * 676 of the 679 are libcurl symbols, and every one of those was diffed
 * against the library: php's value and libcurl's agree on all 676, so naming
 * the SYMBOL rather than the number is what keeps PHL and a php built against
 * the same library in step -- and it is why no version gate is needed. The
 * floor in curl_int.h is 8.5.0, the whole table compiles against it, and a
 * newer libcurl only ever ADDS names.
 *
 * The three that are NOT libcurl symbols are php's own inventions and are the
 * only literals here: CURLOPT_RETURNTRANSFER and CURLOPT_BINARYTRANSFER are
 * php-side options in a range no libcurl option occupies, and
 * CURLOPT_SAFE_UPLOAD is php's -1 sentinel.
 *
 * The values do not all fit an int: CURLAUTH_ONLY is 2147483648 and
 * CURLAUTH_ANY is -17 (libcurl's ~CURLAUTH_DIGEST_IE over an unsigned long),
 * so the row carries a 64-bit value and the expander reads it through the row
 * pointer rather than through SX_INT_TO_PTR.
 */
static const struct CurlConstant {
	const char *zName;
	sxi64 iValue;
} aCurlConst[] = {
	{ "CURLOPT_AUTOREFERER",                     CURLOPT_AUTOREFERER },
	{ "CURLOPT_BINARYTRANSFER",                  19914 },
	{ "CURLOPT_BUFFERSIZE",                      CURLOPT_BUFFERSIZE },
	{ "CURLOPT_CAINFO",                          CURLOPT_CAINFO },
	{ "CURLOPT_CAPATH",                          CURLOPT_CAPATH },
	{ "CURLOPT_CONNECTTIMEOUT",                  CURLOPT_CONNECTTIMEOUT },
	{ "CURLOPT_COOKIE",                          CURLOPT_COOKIE },
	{ "CURLOPT_COOKIEFILE",                      CURLOPT_COOKIEFILE },
	{ "CURLOPT_COOKIEJAR",                       CURLOPT_COOKIEJAR },
	{ "CURLOPT_COOKIESESSION",                   CURLOPT_COOKIESESSION },
	{ "CURLOPT_CRLF",                            CURLOPT_CRLF },
	{ "CURLOPT_CUSTOMREQUEST",                   CURLOPT_CUSTOMREQUEST },
	{ "CURLOPT_DNS_CACHE_TIMEOUT",               CURLOPT_DNS_CACHE_TIMEOUT },
	{ "CURLOPT_DNS_USE_GLOBAL_CACHE",            CURLOPT_DNS_USE_GLOBAL_CACHE },
	{ "CURLOPT_EGDSOCKET",                       CURLOPT_EGDSOCKET },
	{ "CURLOPT_ENCODING",                        CURLOPT_ENCODING },
	{ "CURLOPT_FAILONERROR",                     CURLOPT_FAILONERROR },
	{ "CURLOPT_FILE",                            CURLOPT_FILE },
	{ "CURLOPT_FILETIME",                        CURLOPT_FILETIME },
	{ "CURLOPT_FOLLOWLOCATION",                  CURLOPT_FOLLOWLOCATION },
	{ "CURLOPT_FORBID_REUSE",                    CURLOPT_FORBID_REUSE },
	{ "CURLOPT_FRESH_CONNECT",                   CURLOPT_FRESH_CONNECT },
	{ "CURLOPT_FTPAPPEND",                       CURLOPT_FTPAPPEND },
	{ "CURLOPT_FTPLISTONLY",                     CURLOPT_FTPLISTONLY },
	{ "CURLOPT_FTPPORT",                         CURLOPT_FTPPORT },
	{ "CURLOPT_FTP_USE_EPRT",                    CURLOPT_FTP_USE_EPRT },
	{ "CURLOPT_FTP_USE_EPSV",                    CURLOPT_FTP_USE_EPSV },
	{ "CURLOPT_HEADER",                          CURLOPT_HEADER },
	{ "CURLOPT_HEADERFUNCTION",                  CURLOPT_HEADERFUNCTION },
	{ "CURLOPT_HTTP200ALIASES",                  CURLOPT_HTTP200ALIASES },
	{ "CURLOPT_HTTPGET",                         CURLOPT_HTTPGET },
	{ "CURLOPT_HTTPHEADER",                      CURLOPT_HTTPHEADER },
	{ "CURLOPT_HTTPPROXYTUNNEL",                 CURLOPT_HTTPPROXYTUNNEL },
	{ "CURLOPT_HTTP_VERSION",                    CURLOPT_HTTP_VERSION },
	{ "CURLOPT_INFILE",                          CURLOPT_INFILE },
	{ "CURLOPT_INFILESIZE",                      CURLOPT_INFILESIZE },
	{ "CURLOPT_INFILESIZE_LARGE",                CURLOPT_INFILESIZE_LARGE },
	{ "CURLOPT_INTERFACE",                       CURLOPT_INTERFACE },
	{ "CURLOPT_KRB4LEVEL",                       CURLOPT_KRB4LEVEL },
	{ "CURLOPT_LOW_SPEED_LIMIT",                 CURLOPT_LOW_SPEED_LIMIT },
	{ "CURLOPT_LOW_SPEED_TIME",                  CURLOPT_LOW_SPEED_TIME },
	{ "CURLOPT_MAXCONNECTS",                     CURLOPT_MAXCONNECTS },
	{ "CURLOPT_MAXREDIRS",                       CURLOPT_MAXREDIRS },
	{ "CURLOPT_NETRC",                           CURLOPT_NETRC },
	{ "CURLOPT_NOBODY",                          CURLOPT_NOBODY },
	{ "CURLOPT_NOPROGRESS",                      CURLOPT_NOPROGRESS },
	{ "CURLOPT_NOSIGNAL",                        CURLOPT_NOSIGNAL },
	{ "CURLOPT_PORT",                            CURLOPT_PORT },
	{ "CURLOPT_POST",                            CURLOPT_POST },
	{ "CURLOPT_POSTFIELDS",                      CURLOPT_POSTFIELDS },
	{ "CURLOPT_POSTQUOTE",                       CURLOPT_POSTQUOTE },
	{ "CURLOPT_PREQUOTE",                        CURLOPT_PREQUOTE },
	{ "CURLOPT_PRIVATE",                         CURLOPT_PRIVATE },
	{ "CURLOPT_PROGRESSFUNCTION",                CURLOPT_PROGRESSFUNCTION },
	{ "CURLOPT_PROXY",                           CURLOPT_PROXY },
	{ "CURLOPT_PROXYPORT",                       CURLOPT_PROXYPORT },
	{ "CURLOPT_PROXYTYPE",                       CURLOPT_PROXYTYPE },
	{ "CURLOPT_PROXYUSERPWD",                    CURLOPT_PROXYUSERPWD },
	{ "CURLOPT_PUT",                             CURLOPT_PUT },
	{ "CURLOPT_QUOTE",                           CURLOPT_QUOTE },
	{ "CURLOPT_RANDOM_FILE",                     CURLOPT_RANDOM_FILE },
	{ "CURLOPT_RANGE",                           CURLOPT_RANGE },
	{ "CURLOPT_READDATA",                        CURLOPT_READDATA },
	{ "CURLOPT_READFUNCTION",                    CURLOPT_READFUNCTION },
	{ "CURLOPT_REFERER",                         CURLOPT_REFERER },
	{ "CURLOPT_RESUME_FROM",                     CURLOPT_RESUME_FROM },
	{ "CURLOPT_RETURNTRANSFER",                  19913 },
	{ "CURLOPT_SHARE",                           CURLOPT_SHARE },
	{ "CURLOPT_SSLCERT",                         CURLOPT_SSLCERT },
	{ "CURLOPT_SSLCERTPASSWD",                   CURLOPT_SSLCERTPASSWD },
	{ "CURLOPT_SSLCERTTYPE",                     CURLOPT_SSLCERTTYPE },
	{ "CURLOPT_SSLENGINE",                       CURLOPT_SSLENGINE },
	{ "CURLOPT_SSLENGINE_DEFAULT",               CURLOPT_SSLENGINE_DEFAULT },
	{ "CURLOPT_SSLKEY",                          CURLOPT_SSLKEY },
	{ "CURLOPT_SSLKEYPASSWD",                    CURLOPT_SSLKEYPASSWD },
	{ "CURLOPT_SSLKEYTYPE",                      CURLOPT_SSLKEYTYPE },
	{ "CURLOPT_SSLVERSION",                      CURLOPT_SSLVERSION },
	{ "CURLOPT_SSL_CIPHER_LIST",                 CURLOPT_SSL_CIPHER_LIST },
	{ "CURLOPT_SSL_VERIFYHOST",                  CURLOPT_SSL_VERIFYHOST },
	{ "CURLOPT_SSL_VERIFYPEER",                  CURLOPT_SSL_VERIFYPEER },
	{ "CURLOPT_STDERR",                          CURLOPT_STDERR },
	{ "CURLOPT_TELNETOPTIONS",                   CURLOPT_TELNETOPTIONS },
	{ "CURLOPT_TIMECONDITION",                   CURLOPT_TIMECONDITION },
	{ "CURLOPT_TIMEOUT",                         CURLOPT_TIMEOUT },
	{ "CURLOPT_TIMEVALUE",                       CURLOPT_TIMEVALUE },
	{ "CURLOPT_TRANSFERTEXT",                    CURLOPT_TRANSFERTEXT },
	{ "CURLOPT_UNRESTRICTED_AUTH",               CURLOPT_UNRESTRICTED_AUTH },
	{ "CURLOPT_UPLOAD",                          CURLOPT_UPLOAD },
	{ "CURLOPT_URL",                             CURLOPT_URL },
	{ "CURLOPT_USERAGENT",                       CURLOPT_USERAGENT },
	{ "CURLOPT_USERPWD",                         CURLOPT_USERPWD },
	{ "CURLOPT_VERBOSE",                         CURLOPT_VERBOSE },
	{ "CURLOPT_WRITEFUNCTION",                   CURLOPT_WRITEFUNCTION },
	{ "CURLOPT_WRITEHEADER",                     CURLOPT_WRITEHEADER },
	{ "CURLOPT_XFERINFOFUNCTION",                CURLOPT_XFERINFOFUNCTION },
	{ "CURLOPT_DEBUGFUNCTION",                   CURLOPT_DEBUGFUNCTION },
	{ "CURLINFO_TEXT",                           CURLINFO_TEXT },
	{ "CURLINFO_HEADER_IN",                      CURLINFO_HEADER_IN },
	{ "CURLINFO_DATA_IN",                        CURLINFO_DATA_IN },
	{ "CURLINFO_DATA_OUT",                       CURLINFO_DATA_OUT },
	{ "CURLINFO_SSL_DATA_OUT",                   CURLINFO_SSL_DATA_OUT },
	{ "CURLINFO_SSL_DATA_IN",                    CURLINFO_SSL_DATA_IN },
	{ "CURLE_ABORTED_BY_CALLBACK",               CURLE_ABORTED_BY_CALLBACK },
	{ "CURLE_BAD_CALLING_ORDER",                 CURLE_BAD_CALLING_ORDER },
	{ "CURLE_BAD_CONTENT_ENCODING",              CURLE_BAD_CONTENT_ENCODING },
	{ "CURLE_BAD_DOWNLOAD_RESUME",               CURLE_BAD_DOWNLOAD_RESUME },
	{ "CURLE_BAD_FUNCTION_ARGUMENT",             CURLE_BAD_FUNCTION_ARGUMENT },
	{ "CURLE_BAD_PASSWORD_ENTERED",              CURLE_BAD_PASSWORD_ENTERED },
	{ "CURLE_COULDNT_CONNECT",                   CURLE_COULDNT_CONNECT },
	{ "CURLE_COULDNT_RESOLVE_HOST",              CURLE_COULDNT_RESOLVE_HOST },
	{ "CURLE_COULDNT_RESOLVE_PROXY",             CURLE_COULDNT_RESOLVE_PROXY },
	{ "CURLE_FAILED_INIT",                       CURLE_FAILED_INIT },
	{ "CURLE_FILE_COULDNT_READ_FILE",            CURLE_FILE_COULDNT_READ_FILE },
	{ "CURLE_FTP_ACCESS_DENIED",                 CURLE_FTP_ACCESS_DENIED },
	{ "CURLE_FTP_BAD_DOWNLOAD_RESUME",           CURLE_FTP_BAD_DOWNLOAD_RESUME },
	{ "CURLE_FTP_CANT_GET_HOST",                 CURLE_FTP_CANT_GET_HOST },
	{ "CURLE_FTP_CANT_RECONNECT",                CURLE_FTP_CANT_RECONNECT },
	{ "CURLE_FTP_COULDNT_GET_SIZE",              CURLE_FTP_COULDNT_GET_SIZE },
	{ "CURLE_FTP_COULDNT_RETR_FILE",             CURLE_FTP_COULDNT_RETR_FILE },
	{ "CURLE_FTP_COULDNT_SET_ASCII",             CURLE_FTP_COULDNT_SET_ASCII },
	{ "CURLE_FTP_COULDNT_SET_BINARY",            CURLE_FTP_COULDNT_SET_BINARY },
	{ "CURLE_FTP_COULDNT_STOR_FILE",             CURLE_FTP_COULDNT_STOR_FILE },
	{ "CURLE_FTP_COULDNT_USE_REST",              CURLE_FTP_COULDNT_USE_REST },
	{ "CURLE_FTP_PARTIAL_FILE",                  CURLE_FTP_PARTIAL_FILE },
	{ "CURLE_FTP_PORT_FAILED",                   CURLE_FTP_PORT_FAILED },
	{ "CURLE_FTP_QUOTE_ERROR",                   CURLE_FTP_QUOTE_ERROR },
	{ "CURLE_FTP_USER_PASSWORD_INCORRECT",       CURLE_FTP_USER_PASSWORD_INCORRECT },
	{ "CURLE_FTP_WEIRD_227_FORMAT",              CURLE_FTP_WEIRD_227_FORMAT },
	{ "CURLE_FTP_WEIRD_PASS_REPLY",              CURLE_FTP_WEIRD_PASS_REPLY },
	{ "CURLE_FTP_WEIRD_PASV_REPLY",              CURLE_FTP_WEIRD_PASV_REPLY },
	{ "CURLE_FTP_WEIRD_SERVER_REPLY",            CURLE_FTP_WEIRD_SERVER_REPLY },
	{ "CURLE_FTP_WEIRD_USER_REPLY",              CURLE_FTP_WEIRD_USER_REPLY },
	{ "CURLE_FTP_WRITE_ERROR",                   CURLE_FTP_WRITE_ERROR },
	{ "CURLE_FUNCTION_NOT_FOUND",                CURLE_FUNCTION_NOT_FOUND },
	{ "CURLE_GOT_NOTHING",                       CURLE_GOT_NOTHING },
	{ "CURLE_HTTP_NOT_FOUND",                    CURLE_HTTP_NOT_FOUND },
	{ "CURLE_HTTP_PORT_FAILED",                  CURLE_HTTP_PORT_FAILED },
	{ "CURLE_HTTP_POST_ERROR",                   CURLE_HTTP_POST_ERROR },
	{ "CURLE_HTTP_RANGE_ERROR",                  CURLE_HTTP_RANGE_ERROR },
	{ "CURLE_HTTP_RETURNED_ERROR",               CURLE_HTTP_RETURNED_ERROR },
	{ "CURLE_LDAP_CANNOT_BIND",                  CURLE_LDAP_CANNOT_BIND },
	{ "CURLE_LDAP_SEARCH_FAILED",                CURLE_LDAP_SEARCH_FAILED },
	{ "CURLE_LIBRARY_NOT_FOUND",                 CURLE_LIBRARY_NOT_FOUND },
	{ "CURLE_MALFORMAT_USER",                    CURLE_MALFORMAT_USER },
	{ "CURLE_OBSOLETE",                          CURLE_OBSOLETE },
	{ "CURLE_OK",                                CURLE_OK },
	{ "CURLE_OPERATION_TIMEDOUT",                CURLE_OPERATION_TIMEDOUT },
	{ "CURLE_OPERATION_TIMEOUTED",               CURLE_OPERATION_TIMEOUTED },
	{ "CURLE_OUT_OF_MEMORY",                     CURLE_OUT_OF_MEMORY },
	{ "CURLE_PARTIAL_FILE",                      CURLE_PARTIAL_FILE },
	{ "CURLE_READ_ERROR",                        CURLE_READ_ERROR },
	{ "CURLE_RECV_ERROR",                        CURLE_RECV_ERROR },
	{ "CURLE_SEND_ERROR",                        CURLE_SEND_ERROR },
	{ "CURLE_SHARE_IN_USE",                      CURLE_SHARE_IN_USE },
	{ "CURLE_SSL_CACERT",                        CURLE_SSL_CACERT },
	{ "CURLE_SSL_CERTPROBLEM",                   CURLE_SSL_CERTPROBLEM },
	{ "CURLE_SSL_CIPHER",                        CURLE_SSL_CIPHER },
	{ "CURLE_SSL_CONNECT_ERROR",                 CURLE_SSL_CONNECT_ERROR },
	{ "CURLE_SSL_ENGINE_NOTFOUND",               CURLE_SSL_ENGINE_NOTFOUND },
	{ "CURLE_SSL_ENGINE_SETFAILED",              CURLE_SSL_ENGINE_SETFAILED },
	{ "CURLE_SSL_PEER_CERTIFICATE",              CURLE_SSL_PEER_CERTIFICATE },
	{ "CURLE_SSL_PINNEDPUBKEYNOTMATCH",          CURLE_SSL_PINNEDPUBKEYNOTMATCH },
	{ "CURLE_TELNET_OPTION_SYNTAX",              CURLE_TELNET_OPTION_SYNTAX },
	{ "CURLE_TOO_MANY_REDIRECTS",                CURLE_TOO_MANY_REDIRECTS },
	{ "CURLE_UNKNOWN_TELNET_OPTION",             CURLE_UNKNOWN_TELNET_OPTION },
	{ "CURLE_UNSUPPORTED_PROTOCOL",              CURLE_UNSUPPORTED_PROTOCOL },
	{ "CURLE_URL_MALFORMAT",                     CURLE_URL_MALFORMAT },
	{ "CURLE_URL_MALFORMAT_USER",                CURLE_URL_MALFORMAT_USER },
	{ "CURLE_WRITE_ERROR",                       CURLE_WRITE_ERROR },
	{ "CURLINFO_CONNECT_TIME",                   CURLINFO_CONNECT_TIME },
	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD",        CURLINFO_CONTENT_LENGTH_DOWNLOAD },
	{ "CURLINFO_CONTENT_LENGTH_UPLOAD",          CURLINFO_CONTENT_LENGTH_UPLOAD },
	{ "CURLINFO_CONTENT_TYPE",                   CURLINFO_CONTENT_TYPE },
	{ "CURLINFO_EFFECTIVE_URL",                  CURLINFO_EFFECTIVE_URL },
	{ "CURLINFO_FILETIME",                       CURLINFO_FILETIME },
	{ "CURLINFO_HEADER_OUT",                     CURLINFO_HEADER_OUT },
	{ "CURLINFO_HEADER_SIZE",                    CURLINFO_HEADER_SIZE },
	{ "CURLINFO_HTTP_CODE",                      CURLINFO_HTTP_CODE },
	{ "CURLINFO_LASTONE",                        CURLINFO_LASTONE },
	{ "CURLINFO_NAMELOOKUP_TIME",                CURLINFO_NAMELOOKUP_TIME },
	{ "CURLINFO_PRETRANSFER_TIME",               CURLINFO_PRETRANSFER_TIME },
	{ "CURLINFO_PRIVATE",                        CURLINFO_PRIVATE },
	{ "CURLINFO_REDIRECT_COUNT",                 CURLINFO_REDIRECT_COUNT },
	{ "CURLINFO_REDIRECT_TIME",                  CURLINFO_REDIRECT_TIME },
	{ "CURLINFO_REQUEST_SIZE",                   CURLINFO_REQUEST_SIZE },
	{ "CURLINFO_SIZE_DOWNLOAD",                  CURLINFO_SIZE_DOWNLOAD },
	{ "CURLINFO_SIZE_UPLOAD",                    CURLINFO_SIZE_UPLOAD },
	{ "CURLINFO_SPEED_DOWNLOAD",                 CURLINFO_SPEED_DOWNLOAD },
	{ "CURLINFO_SPEED_UPLOAD",                   CURLINFO_SPEED_UPLOAD },
	{ "CURLINFO_SSL_VERIFYRESULT",               CURLINFO_SSL_VERIFYRESULT },
	{ "CURLINFO_STARTTRANSFER_TIME",             CURLINFO_STARTTRANSFER_TIME },
	{ "CURLINFO_TOTAL_TIME",                     CURLINFO_TOTAL_TIME },
	{ "CURLINFO_EFFECTIVE_METHOD",               CURLINFO_EFFECTIVE_METHOD },
	{ "CURLINFO_CAPATH",                         CURLINFO_CAPATH },
	{ "CURLINFO_CAINFO",                         CURLINFO_CAINFO },
	{ "CURLMSG_DONE",                            CURLMSG_DONE },
	{ "CURLVERSION_NOW",                         CURLVERSION_NOW },
	{ "CURLM_BAD_EASY_HANDLE",                   CURLM_BAD_EASY_HANDLE },
	{ "CURLM_BAD_HANDLE",                        CURLM_BAD_HANDLE },
	{ "CURLM_CALL_MULTI_PERFORM",                CURLM_CALL_MULTI_PERFORM },
	{ "CURLM_INTERNAL_ERROR",                    CURLM_INTERNAL_ERROR },
	{ "CURLM_OK",                                CURLM_OK },
	{ "CURLM_OUT_OF_MEMORY",                     CURLM_OUT_OF_MEMORY },
	{ "CURLM_ADDED_ALREADY",                     CURLM_ADDED_ALREADY },
	{ "CURLPROXY_HTTP",                          CURLPROXY_HTTP },
	{ "CURLPROXY_SOCKS4",                        CURLPROXY_SOCKS4 },
	{ "CURLPROXY_SOCKS5",                        CURLPROXY_SOCKS5 },
	{ "CURLSHOPT_NONE",                          CURLSHOPT_NONE },
	{ "CURLSHOPT_SHARE",                         CURLSHOPT_SHARE },
	{ "CURLSHOPT_UNSHARE",                       CURLSHOPT_UNSHARE },
	{ "CURL_HTTP_VERSION_1_0",                   CURL_HTTP_VERSION_1_0 },
	{ "CURL_HTTP_VERSION_1_1",                   CURL_HTTP_VERSION_1_1 },
	{ "CURL_HTTP_VERSION_NONE",                  CURL_HTTP_VERSION_NONE },
	{ "CURL_LOCK_DATA_COOKIE",                   CURL_LOCK_DATA_COOKIE },
	{ "CURL_LOCK_DATA_DNS",                      CURL_LOCK_DATA_DNS },
	{ "CURL_LOCK_DATA_SSL_SESSION",              CURL_LOCK_DATA_SSL_SESSION },
	{ "CURL_NETRC_IGNORED",                      CURL_NETRC_IGNORED },
	{ "CURL_NETRC_OPTIONAL",                     CURL_NETRC_OPTIONAL },
	{ "CURL_NETRC_REQUIRED",                     CURL_NETRC_REQUIRED },
	{ "CURL_SSLVERSION_DEFAULT",                 CURL_SSLVERSION_DEFAULT },
	{ "CURL_SSLVERSION_SSLv2",                   CURL_SSLVERSION_SSLv2 },
	{ "CURL_SSLVERSION_SSLv3",                   CURL_SSLVERSION_SSLv3 },
	{ "CURL_SSLVERSION_TLSv1",                   CURL_SSLVERSION_TLSv1 },
	{ "CURL_TIMECOND_IFMODSINCE",                CURL_TIMECOND_IFMODSINCE },
	{ "CURL_TIMECOND_IFUNMODSINCE",              CURL_TIMECOND_IFUNMODSINCE },
	{ "CURL_TIMECOND_LASTMOD",                   CURL_TIMECOND_LASTMOD },
	{ "CURL_TIMECOND_NONE",                      CURL_TIMECOND_NONE },
	{ "CURL_VERSION_ASYNCHDNS",                  CURL_VERSION_ASYNCHDNS },
	{ "CURL_VERSION_CONV",                       CURL_VERSION_CONV },
	{ "CURL_VERSION_DEBUG",                      CURL_VERSION_DEBUG },
	{ "CURL_VERSION_GSSNEGOTIATE",               CURL_VERSION_GSSNEGOTIATE },
	{ "CURL_VERSION_IDN",                        CURL_VERSION_IDN },
	{ "CURL_VERSION_IPV6",                       CURL_VERSION_IPV6 },
	{ "CURL_VERSION_KERBEROS4",                  CURL_VERSION_KERBEROS4 },
	{ "CURL_VERSION_LARGEFILE",                  CURL_VERSION_LARGEFILE },
	{ "CURL_VERSION_LIBZ",                       CURL_VERSION_LIBZ },
	{ "CURL_VERSION_NTLM",                       CURL_VERSION_NTLM },
	{ "CURL_VERSION_SPNEGO",                     CURL_VERSION_SPNEGO },
	{ "CURL_VERSION_SSL",                        CURL_VERSION_SSL },
	{ "CURL_VERSION_SSPI",                       CURL_VERSION_SSPI },
	{ "CURLOPT_HTTPAUTH",                        CURLOPT_HTTPAUTH },
	{ "CURLAUTH_ANY",                            CURLAUTH_ANY },
	{ "CURLAUTH_ANYSAFE",                        CURLAUTH_ANYSAFE },
	{ "CURLAUTH_BASIC",                          CURLAUTH_BASIC },
	{ "CURLAUTH_DIGEST",                         CURLAUTH_DIGEST },
	{ "CURLAUTH_GSSNEGOTIATE",                   CURLAUTH_GSSNEGOTIATE },
	{ "CURLAUTH_NONE",                           CURLAUTH_NONE },
	{ "CURLAUTH_NTLM",                           CURLAUTH_NTLM },
	{ "CURLINFO_HTTP_CONNECTCODE",               CURLINFO_HTTP_CONNECTCODE },
	{ "CURLOPT_FTP_CREATE_MISSING_DIRS",         CURLOPT_FTP_CREATE_MISSING_DIRS },
	{ "CURLOPT_PROXYAUTH",                       CURLOPT_PROXYAUTH },
	{ "CURLE_FILESIZE_EXCEEDED",                 CURLE_FILESIZE_EXCEEDED },
	{ "CURLE_LDAP_INVALID_URL",                  CURLE_LDAP_INVALID_URL },
	{ "CURLINFO_HTTPAUTH_AVAIL",                 CURLINFO_HTTPAUTH_AVAIL },
	{ "CURLINFO_RESPONSE_CODE",                  CURLINFO_RESPONSE_CODE },
	{ "CURLINFO_PROXYAUTH_AVAIL",                CURLINFO_PROXYAUTH_AVAIL },
	{ "CURLOPT_FTP_RESPONSE_TIMEOUT",            CURLOPT_FTP_RESPONSE_TIMEOUT },
	{ "CURLOPT_SERVER_RESPONSE_TIMEOUT",         CURLOPT_SERVER_RESPONSE_TIMEOUT },
	{ "CURLOPT_IPRESOLVE",                       CURLOPT_IPRESOLVE },
	{ "CURLOPT_MAXFILESIZE",                     CURLOPT_MAXFILESIZE },
	{ "CURL_IPRESOLVE_V4",                       CURL_IPRESOLVE_V4 },
	{ "CURL_IPRESOLVE_V6",                       CURL_IPRESOLVE_V6 },
	{ "CURL_IPRESOLVE_WHATEVER",                 CURL_IPRESOLVE_WHATEVER },
	{ "CURLE_FTP_SSL_FAILED",                    CURLE_FTP_SSL_FAILED },
	{ "CURLFTPSSL_ALL",                          CURLFTPSSL_ALL },
	{ "CURLFTPSSL_CONTROL",                      CURLFTPSSL_CONTROL },
	{ "CURLFTPSSL_NONE",                         CURLFTPSSL_NONE },
	{ "CURLFTPSSL_TRY",                          CURLFTPSSL_TRY },
	{ "CURLOPT_FTP_SSL",                         CURLOPT_FTP_SSL },
	{ "CURLOPT_NETRC_FILE",                      CURLOPT_NETRC_FILE },
	{ "CURLOPT_MAXFILESIZE_LARGE",               CURLOPT_MAXFILESIZE_LARGE },
	{ "CURLOPT_TCP_NODELAY",                     CURLOPT_TCP_NODELAY },
	{ "CURLFTPAUTH_DEFAULT",                     CURLFTPAUTH_DEFAULT },
	{ "CURLFTPAUTH_SSL",                         CURLFTPAUTH_SSL },
	{ "CURLFTPAUTH_TLS",                         CURLFTPAUTH_TLS },
	{ "CURLOPT_FTPSSLAUTH",                      CURLOPT_FTPSSLAUTH },
	{ "CURLOPT_FTP_ACCOUNT",                     CURLOPT_FTP_ACCOUNT },
	{ "CURLINFO_OS_ERRNO",                       CURLINFO_OS_ERRNO },
	{ "CURLINFO_NUM_CONNECTS",                   CURLINFO_NUM_CONNECTS },
	{ "CURLINFO_SSL_ENGINES",                    CURLINFO_SSL_ENGINES },
	{ "CURLINFO_COOKIELIST",                     CURLINFO_COOKIELIST },
	{ "CURLOPT_COOKIELIST",                      CURLOPT_COOKIELIST },
	{ "CURLOPT_IGNORE_CONTENT_LENGTH",           CURLOPT_IGNORE_CONTENT_LENGTH },
	{ "CURLOPT_FTP_SKIP_PASV_IP",                CURLOPT_FTP_SKIP_PASV_IP },
	{ "CURLOPT_FTP_FILEMETHOD",                  CURLOPT_FTP_FILEMETHOD },
	{ "CURLOPT_CONNECT_ONLY",                    CURLOPT_CONNECT_ONLY },
	{ "CURLOPT_LOCALPORT",                       CURLOPT_LOCALPORT },
	{ "CURLOPT_LOCALPORTRANGE",                  CURLOPT_LOCALPORTRANGE },
	{ "CURLFTPMETHOD_DEFAULT",                   CURLFTPMETHOD_DEFAULT },
	{ "CURLFTPMETHOD_MULTICWD",                  CURLFTPMETHOD_MULTICWD },
	{ "CURLFTPMETHOD_NOCWD",                     CURLFTPMETHOD_NOCWD },
	{ "CURLFTPMETHOD_SINGLECWD",                 CURLFTPMETHOD_SINGLECWD },
	{ "CURLINFO_FTP_ENTRY_PATH",                 CURLINFO_FTP_ENTRY_PATH },
	{ "CURLOPT_FTP_ALTERNATIVE_TO_USER",         CURLOPT_FTP_ALTERNATIVE_TO_USER },
	{ "CURLOPT_MAX_RECV_SPEED_LARGE",            CURLOPT_MAX_RECV_SPEED_LARGE },
	{ "CURLOPT_MAX_SEND_SPEED_LARGE",            CURLOPT_MAX_SEND_SPEED_LARGE },
	{ "CURLE_SSL_CACERT_BADFILE",                CURLE_SSL_CACERT_BADFILE },
	{ "CURLOPT_SSL_SESSIONID_CACHE",             CURLOPT_SSL_SESSIONID_CACHE },
	{ "CURLMOPT_PIPELINING",                     CURLMOPT_PIPELINING },
	{ "CURLE_SSH",                               CURLE_SSH },
	{ "CURLOPT_FTP_SSL_CCC",                     CURLOPT_FTP_SSL_CCC },
	{ "CURLOPT_SSH_AUTH_TYPES",                  CURLOPT_SSH_AUTH_TYPES },
	{ "CURLOPT_SSH_PRIVATE_KEYFILE",             CURLOPT_SSH_PRIVATE_KEYFILE },
	{ "CURLOPT_SSH_PUBLIC_KEYFILE",              CURLOPT_SSH_PUBLIC_KEYFILE },
	{ "CURLFTPSSL_CCC_ACTIVE",                   CURLFTPSSL_CCC_ACTIVE },
	{ "CURLFTPSSL_CCC_NONE",                     CURLFTPSSL_CCC_NONE },
	{ "CURLFTPSSL_CCC_PASSIVE",                  CURLFTPSSL_CCC_PASSIVE },
	{ "CURLOPT_CONNECTTIMEOUT_MS",               CURLOPT_CONNECTTIMEOUT_MS },
	{ "CURLOPT_HTTP_CONTENT_DECODING",           CURLOPT_HTTP_CONTENT_DECODING },
	{ "CURLOPT_HTTP_TRANSFER_DECODING",          CURLOPT_HTTP_TRANSFER_DECODING },
	{ "CURLOPT_TIMEOUT_MS",                      CURLOPT_TIMEOUT_MS },
	{ "CURLMOPT_MAXCONNECTS",                    CURLMOPT_MAXCONNECTS },
	{ "CURLOPT_KRBLEVEL",                        CURLOPT_KRBLEVEL },
	{ "CURLOPT_NEW_DIRECTORY_PERMS",             CURLOPT_NEW_DIRECTORY_PERMS },
	{ "CURLOPT_NEW_FILE_PERMS",                  CURLOPT_NEW_FILE_PERMS },
	{ "CURLOPT_APPEND",                          CURLOPT_APPEND },
	{ "CURLOPT_DIRLISTONLY",                     CURLOPT_DIRLISTONLY },
	{ "CURLOPT_USE_SSL",                         CURLOPT_USE_SSL },
	{ "CURLUSESSL_ALL",                          CURLUSESSL_ALL },
	{ "CURLUSESSL_CONTROL",                      CURLUSESSL_CONTROL },
	{ "CURLUSESSL_NONE",                         CURLUSESSL_NONE },
	{ "CURLUSESSL_TRY",                          CURLUSESSL_TRY },
	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_MD5",         CURLOPT_SSH_HOST_PUBLIC_KEY_MD5 },
	{ "CURLOPT_PROXY_TRANSFER_MODE",             CURLOPT_PROXY_TRANSFER_MODE },
	{ "CURLPAUSE_ALL",                           CURLPAUSE_ALL },
	{ "CURLPAUSE_CONT",                          CURLPAUSE_CONT },
	{ "CURLPAUSE_RECV",                          CURLPAUSE_RECV },
	{ "CURLPAUSE_RECV_CONT",                     CURLPAUSE_RECV_CONT },
	{ "CURLPAUSE_SEND",                          CURLPAUSE_SEND },
	{ "CURLPAUSE_SEND_CONT",                     CURLPAUSE_SEND_CONT },
	{ "CURL_READFUNC_PAUSE",                     CURL_READFUNC_PAUSE },
	{ "CURL_WRITEFUNC_PAUSE",                    CURL_WRITEFUNC_PAUSE },
	{ "CURLPROXY_SOCKS4A",                       CURLPROXY_SOCKS4A },
	{ "CURLPROXY_SOCKS5_HOSTNAME",               CURLPROXY_SOCKS5_HOSTNAME },
	{ "CURLINFO_REDIRECT_URL",                   CURLINFO_REDIRECT_URL },
	{ "CURLINFO_APPCONNECT_TIME",                CURLINFO_APPCONNECT_TIME },
	{ "CURLINFO_PRIMARY_IP",                     CURLINFO_PRIMARY_IP },
	{ "CURLOPT_ADDRESS_SCOPE",                   CURLOPT_ADDRESS_SCOPE },
	{ "CURLOPT_CRLFILE",                         CURLOPT_CRLFILE },
	{ "CURLOPT_ISSUERCERT",                      CURLOPT_ISSUERCERT },
	{ "CURLOPT_KEYPASSWD",                       CURLOPT_KEYPASSWD },
	{ "CURLSSH_AUTH_ANY",                        CURLSSH_AUTH_ANY },
	{ "CURLSSH_AUTH_DEFAULT",                    CURLSSH_AUTH_DEFAULT },
	{ "CURLSSH_AUTH_HOST",                       CURLSSH_AUTH_HOST },
	{ "CURLSSH_AUTH_KEYBOARD",                   CURLSSH_AUTH_KEYBOARD },
	{ "CURLSSH_AUTH_NONE",                       CURLSSH_AUTH_NONE },
	{ "CURLSSH_AUTH_PASSWORD",                   CURLSSH_AUTH_PASSWORD },
	{ "CURLSSH_AUTH_PUBLICKEY",                  CURLSSH_AUTH_PUBLICKEY },
	{ "CURLINFO_CERTINFO",                       CURLINFO_CERTINFO },
	{ "CURLOPT_CERTINFO",                        CURLOPT_CERTINFO },
	{ "CURLOPT_PASSWORD",                        CURLOPT_PASSWORD },
	{ "CURLOPT_POSTREDIR",                       CURLOPT_POSTREDIR },
	{ "CURLOPT_PROXYPASSWORD",                   CURLOPT_PROXYPASSWORD },
	{ "CURLOPT_PROXYUSERNAME",                   CURLOPT_PROXYUSERNAME },
	{ "CURLOPT_USERNAME",                        CURLOPT_USERNAME },
	{ "CURL_REDIR_POST_301",                     CURL_REDIR_POST_301 },
	{ "CURL_REDIR_POST_302",                     CURL_REDIR_POST_302 },
	{ "CURL_REDIR_POST_ALL",                     CURL_REDIR_POST_ALL },
	{ "CURLAUTH_DIGEST_IE",                      CURLAUTH_DIGEST_IE },
	{ "CURLINFO_CONDITION_UNMET",                CURLINFO_CONDITION_UNMET },
	{ "CURLOPT_NOPROXY",                         CURLOPT_NOPROXY },
	{ "CURLOPT_PROTOCOLS",                       CURLOPT_PROTOCOLS },
	{ "CURLOPT_REDIR_PROTOCOLS",                 CURLOPT_REDIR_PROTOCOLS },
	{ "CURLOPT_SOCKS5_GSSAPI_NEC",               CURLOPT_SOCKS5_GSSAPI_NEC },
	{ "CURLOPT_SOCKS5_GSSAPI_SERVICE",           CURLOPT_SOCKS5_GSSAPI_SERVICE },
	{ "CURLOPT_TFTP_BLKSIZE",                    CURLOPT_TFTP_BLKSIZE },
	{ "CURLPROTO_ALL",                           CURLPROTO_ALL },
	{ "CURLPROTO_DICT",                          CURLPROTO_DICT },
	{ "CURLPROTO_FILE",                          CURLPROTO_FILE },
	{ "CURLPROTO_FTP",                           CURLPROTO_FTP },
	{ "CURLPROTO_FTPS",                          CURLPROTO_FTPS },
	{ "CURLPROTO_HTTP",                          CURLPROTO_HTTP },
	{ "CURLPROTO_HTTPS",                         CURLPROTO_HTTPS },
	{ "CURLPROTO_LDAP",                          CURLPROTO_LDAP },
	{ "CURLPROTO_LDAPS",                         CURLPROTO_LDAPS },
	{ "CURLPROTO_SCP",                           CURLPROTO_SCP },
	{ "CURLPROTO_SFTP",                          CURLPROTO_SFTP },
	{ "CURLPROTO_TELNET",                        CURLPROTO_TELNET },
	{ "CURLPROTO_TFTP",                          CURLPROTO_TFTP },
	{ "CURLPROXY_HTTP_1_0",                      CURLPROXY_HTTP_1_0 },
	{ "CURLFTP_CREATE_DIR",                      CURLFTP_CREATE_DIR },
	{ "CURLFTP_CREATE_DIR_NONE",                 CURLFTP_CREATE_DIR_NONE },
	{ "CURLFTP_CREATE_DIR_RETRY",                CURLFTP_CREATE_DIR_RETRY },
	{ "CURL_VERSION_CURLDEBUG",                  CURL_VERSION_CURLDEBUG },
	{ "CURLOPT_SSH_KNOWNHOSTS",                  CURLOPT_SSH_KNOWNHOSTS },
	{ "CURLKHMATCH_OK",                          CURLKHMATCH_OK },
	{ "CURLKHMATCH_MISMATCH",                    CURLKHMATCH_MISMATCH },
	{ "CURLKHMATCH_MISSING",                     CURLKHMATCH_MISSING },
	{ "CURLKHMATCH_LAST",                        CURLKHMATCH_LAST },
	{ "CURLINFO_RTSP_CLIENT_CSEQ",               CURLINFO_RTSP_CLIENT_CSEQ },
	{ "CURLINFO_RTSP_CSEQ_RECV",                 CURLINFO_RTSP_CSEQ_RECV },
	{ "CURLINFO_RTSP_SERVER_CSEQ",               CURLINFO_RTSP_SERVER_CSEQ },
	{ "CURLINFO_RTSP_SESSION_ID",                CURLINFO_RTSP_SESSION_ID },
	{ "CURLOPT_FTP_USE_PRET",                    CURLOPT_FTP_USE_PRET },
	{ "CURLOPT_MAIL_FROM",                       CURLOPT_MAIL_FROM },
	{ "CURLOPT_MAIL_RCPT",                       CURLOPT_MAIL_RCPT },
	{ "CURLOPT_RTSP_CLIENT_CSEQ",                CURLOPT_RTSP_CLIENT_CSEQ },
	{ "CURLOPT_RTSP_REQUEST",                    CURLOPT_RTSP_REQUEST },
	{ "CURLOPT_RTSP_SERVER_CSEQ",                CURLOPT_RTSP_SERVER_CSEQ },
	{ "CURLOPT_RTSP_SESSION_ID",                 CURLOPT_RTSP_SESSION_ID },
	{ "CURLOPT_RTSP_STREAM_URI",                 CURLOPT_RTSP_STREAM_URI },
	{ "CURLOPT_RTSP_TRANSPORT",                  CURLOPT_RTSP_TRANSPORT },
	{ "CURLPROTO_IMAP",                          CURLPROTO_IMAP },
	{ "CURLPROTO_IMAPS",                         CURLPROTO_IMAPS },
	{ "CURLPROTO_POP3",                          CURLPROTO_POP3 },
	{ "CURLPROTO_POP3S",                         CURLPROTO_POP3S },
	{ "CURLPROTO_RTSP",                          CURLPROTO_RTSP },
	{ "CURLPROTO_SMTP",                          CURLPROTO_SMTP },
	{ "CURLPROTO_SMTPS",                         CURLPROTO_SMTPS },
	{ "CURL_RTSPREQ_ANNOUNCE",                   CURL_RTSPREQ_ANNOUNCE },
	{ "CURL_RTSPREQ_DESCRIBE",                   CURL_RTSPREQ_DESCRIBE },
	{ "CURL_RTSPREQ_GET_PARAMETER",              CURL_RTSPREQ_GET_PARAMETER },
	{ "CURL_RTSPREQ_OPTIONS",                    CURL_RTSPREQ_OPTIONS },
	{ "CURL_RTSPREQ_PAUSE",                      CURL_RTSPREQ_PAUSE },
	{ "CURL_RTSPREQ_PLAY",                       CURL_RTSPREQ_PLAY },
	{ "CURL_RTSPREQ_RECEIVE",                    CURL_RTSPREQ_RECEIVE },
	{ "CURL_RTSPREQ_RECORD",                     CURL_RTSPREQ_RECORD },
	{ "CURL_RTSPREQ_SET_PARAMETER",              CURL_RTSPREQ_SET_PARAMETER },
	{ "CURL_RTSPREQ_SETUP",                      CURL_RTSPREQ_SETUP },
	{ "CURL_RTSPREQ_TEARDOWN",                   CURL_RTSPREQ_TEARDOWN },
	{ "CURLINFO_LOCAL_IP",                       CURLINFO_LOCAL_IP },
	{ "CURLINFO_LOCAL_PORT",                     CURLINFO_LOCAL_PORT },
	{ "CURLINFO_PRIMARY_PORT",                   CURLINFO_PRIMARY_PORT },
	{ "CURLOPT_FNMATCH_FUNCTION",                CURLOPT_FNMATCH_FUNCTION },
	{ "CURLOPT_WILDCARDMATCH",                   CURLOPT_WILDCARDMATCH },
	{ "CURLPROTO_RTMP",                          CURLPROTO_RTMP },
	{ "CURLPROTO_RTMPE",                         CURLPROTO_RTMPE },
	{ "CURLPROTO_RTMPS",                         CURLPROTO_RTMPS },
	{ "CURLPROTO_RTMPT",                         CURLPROTO_RTMPT },
	{ "CURLPROTO_RTMPTE",                        CURLPROTO_RTMPTE },
	{ "CURLPROTO_RTMPTS",                        CURLPROTO_RTMPTS },
	{ "CURL_FNMATCHFUNC_FAIL",                   CURL_FNMATCHFUNC_FAIL },
	{ "CURL_FNMATCHFUNC_MATCH",                  CURL_FNMATCHFUNC_MATCH },
	{ "CURL_FNMATCHFUNC_NOMATCH",                CURL_FNMATCHFUNC_NOMATCH },
	{ "CURLPROTO_GOPHER",                        CURLPROTO_GOPHER },
	{ "CURLAUTH_ONLY",                           CURLAUTH_ONLY },
	{ "CURLOPT_RESOLVE",                         CURLOPT_RESOLVE },
	{ "CURLOPT_TLSAUTH_PASSWORD",                CURLOPT_TLSAUTH_PASSWORD },
	{ "CURLOPT_TLSAUTH_TYPE",                    CURLOPT_TLSAUTH_TYPE },
	{ "CURLOPT_TLSAUTH_USERNAME",                CURLOPT_TLSAUTH_USERNAME },
	{ "CURL_TLSAUTH_SRP",                        CURL_TLSAUTH_SRP },
	{ "CURL_VERSION_TLSAUTH_SRP",                CURL_VERSION_TLSAUTH_SRP },
	{ "CURLOPT_ACCEPT_ENCODING",                 CURLOPT_ACCEPT_ENCODING },
	{ "CURLOPT_TRANSFER_ENCODING",               CURLOPT_TRANSFER_ENCODING },
	{ "CURLAUTH_NTLM_WB",                        CURLAUTH_NTLM_WB },
	{ "CURLGSSAPI_DELEGATION_FLAG",              CURLGSSAPI_DELEGATION_FLAG },
	{ "CURLGSSAPI_DELEGATION_POLICY_FLAG",       CURLGSSAPI_DELEGATION_POLICY_FLAG },
	{ "CURLOPT_GSSAPI_DELEGATION",               CURLOPT_GSSAPI_DELEGATION },
	{ "CURL_VERSION_NTLM_WB",                    CURL_VERSION_NTLM_WB },
	{ "CURLOPT_ACCEPTTIMEOUT_MS",                CURLOPT_ACCEPTTIMEOUT_MS },
	{ "CURLOPT_DNS_SERVERS",                     CURLOPT_DNS_SERVERS },
	{ "CURLOPT_MAIL_AUTH",                       CURLOPT_MAIL_AUTH },
	{ "CURLOPT_SSL_OPTIONS",                     CURLOPT_SSL_OPTIONS },
	{ "CURLOPT_TCP_KEEPALIVE",                   CURLOPT_TCP_KEEPALIVE },
	{ "CURLOPT_TCP_KEEPIDLE",                    CURLOPT_TCP_KEEPIDLE },
	{ "CURLOPT_TCP_KEEPINTVL",                   CURLOPT_TCP_KEEPINTVL },
	{ "CURLSSLOPT_ALLOW_BEAST",                  CURLSSLOPT_ALLOW_BEAST },
	{ "CURL_REDIR_POST_303",                     CURL_REDIR_POST_303 },
	{ "CURLSSH_AUTH_AGENT",                      CURLSSH_AUTH_AGENT },
	{ "CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE",      CURLMOPT_CHUNK_LENGTH_PENALTY_SIZE },
	{ "CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE",    CURLMOPT_CONTENT_LENGTH_PENALTY_SIZE },
	{ "CURLMOPT_MAX_HOST_CONNECTIONS",           CURLMOPT_MAX_HOST_CONNECTIONS },
	{ "CURLMOPT_MAX_PIPELINE_LENGTH",            CURLMOPT_MAX_PIPELINE_LENGTH },
	{ "CURLMOPT_MAX_TOTAL_CONNECTIONS",          CURLMOPT_MAX_TOTAL_CONNECTIONS },
	{ "CURLOPT_SASL_IR",                         CURLOPT_SASL_IR },
	{ "CURLOPT_DNS_INTERFACE",                   CURLOPT_DNS_INTERFACE },
	{ "CURLOPT_DNS_LOCAL_IP4",                   CURLOPT_DNS_LOCAL_IP4 },
	{ "CURLOPT_DNS_LOCAL_IP6",                   CURLOPT_DNS_LOCAL_IP6 },
	{ "CURLOPT_XOAUTH2_BEARER",                  CURLOPT_XOAUTH2_BEARER },
	{ "CURL_HTTP_VERSION_2_0",                   CURL_HTTP_VERSION_2_0 },
	{ "CURL_VERSION_HTTP2",                      CURL_VERSION_HTTP2 },
	{ "CURLOPT_LOGIN_OPTIONS",                   CURLOPT_LOGIN_OPTIONS },
	{ "CURL_SSLVERSION_TLSv1_0",                 CURL_SSLVERSION_TLSv1_0 },
	{ "CURL_SSLVERSION_TLSv1_1",                 CURL_SSLVERSION_TLSv1_1 },
	{ "CURL_SSLVERSION_TLSv1_2",                 CURL_SSLVERSION_TLSv1_2 },
	{ "CURLOPT_EXPECT_100_TIMEOUT_MS",           CURLOPT_EXPECT_100_TIMEOUT_MS },
	{ "CURLOPT_SSL_ENABLE_ALPN",                 CURLOPT_SSL_ENABLE_ALPN },
	{ "CURLOPT_SSL_ENABLE_NPN",                  CURLOPT_SSL_ENABLE_NPN },
	{ "CURLHEADER_SEPARATE",                     CURLHEADER_SEPARATE },
	{ "CURLHEADER_UNIFIED",                      CURLHEADER_UNIFIED },
	{ "CURLOPT_HEADEROPT",                       CURLOPT_HEADEROPT },
	{ "CURLOPT_PROXYHEADER",                     CURLOPT_PROXYHEADER },
	{ "CURLAUTH_NEGOTIATE",                      CURLAUTH_NEGOTIATE },
	{ "CURL_VERSION_GSSAPI",                     CURL_VERSION_GSSAPI },
	{ "CURLOPT_PINNEDPUBLICKEY",                 CURLOPT_PINNEDPUBLICKEY },
	{ "CURLOPT_UNIX_SOCKET_PATH",                CURLOPT_UNIX_SOCKET_PATH },
	{ "CURLPROTO_SMB",                           CURLPROTO_SMB },
	{ "CURLPROTO_SMBS",                          CURLPROTO_SMBS },
	{ "CURL_VERSION_KERBEROS5",                  CURL_VERSION_KERBEROS5 },
	{ "CURL_VERSION_UNIX_SOCKETS",               CURL_VERSION_UNIX_SOCKETS },
	{ "CURLOPT_SSL_VERIFYSTATUS",                CURLOPT_SSL_VERIFYSTATUS },
	{ "CURLOPT_PATH_AS_IS",                      CURLOPT_PATH_AS_IS },
	{ "CURLOPT_SSL_FALSESTART",                  CURLOPT_SSL_FALSESTART },
	{ "CURL_HTTP_VERSION_2",                     CURL_HTTP_VERSION_2 },
	{ "CURLOPT_PIPEWAIT",                        CURLOPT_PIPEWAIT },
	{ "CURLOPT_PROXY_SERVICE_NAME",              CURLOPT_PROXY_SERVICE_NAME },
	{ "CURLOPT_SERVICE_NAME",                    CURLOPT_SERVICE_NAME },
	{ "CURLPIPE_NOTHING",                        CURLPIPE_NOTHING },
	{ "CURLPIPE_HTTP1",                          CURLPIPE_HTTP1 },
	{ "CURLPIPE_MULTIPLEX",                      CURLPIPE_MULTIPLEX },
	{ "CURLSSLOPT_NO_REVOKE",                    CURLSSLOPT_NO_REVOKE },
	{ "CURLOPT_DEFAULT_PROTOCOL",                CURLOPT_DEFAULT_PROTOCOL },
	{ "CURLOPT_STREAM_WEIGHT",                   CURLOPT_STREAM_WEIGHT },
	{ "CURLMOPT_PUSHFUNCTION",                   CURLMOPT_PUSHFUNCTION },
	{ "CURL_PUSH_OK",                            CURL_PUSH_OK },
	{ "CURL_PUSH_DENY",                          CURL_PUSH_DENY },
	{ "CURL_HTTP_VERSION_2TLS",                  CURL_HTTP_VERSION_2TLS },
	{ "CURL_VERSION_PSL",                        CURL_VERSION_PSL },
	{ "CURLOPT_TFTP_NO_OPTIONS",                 CURLOPT_TFTP_NO_OPTIONS },
	{ "CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE",     CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE },
	{ "CURLOPT_CONNECT_TO",                      CURLOPT_CONNECT_TO },
	{ "CURLOPT_TCP_FASTOPEN",                    CURLOPT_TCP_FASTOPEN },
	{ "CURLINFO_HTTP_VERSION",                   CURLINFO_HTTP_VERSION },
	{ "CURLE_WEIRD_SERVER_REPLY",                CURLE_WEIRD_SERVER_REPLY },
	{ "CURLOPT_KEEP_SENDING_ON_ERROR",           CURLOPT_KEEP_SENDING_ON_ERROR },
	{ "CURL_SSLVERSION_TLSv1_3",                 CURL_SSLVERSION_TLSv1_3 },
	{ "CURL_VERSION_HTTPS_PROXY",                CURL_VERSION_HTTPS_PROXY },
	{ "CURLINFO_PROTOCOL",                       CURLINFO_PROTOCOL },
	{ "CURLINFO_PROXY_SSL_VERIFYRESULT",         CURLINFO_PROXY_SSL_VERIFYRESULT },
	{ "CURLINFO_SCHEME",                         CURLINFO_SCHEME },
	{ "CURLOPT_PRE_PROXY",                       CURLOPT_PRE_PROXY },
	{ "CURLOPT_PROXY_CAINFO",                    CURLOPT_PROXY_CAINFO },
	{ "CURLOPT_PROXY_CAPATH",                    CURLOPT_PROXY_CAPATH },
	{ "CURLOPT_PROXY_CRLFILE",                   CURLOPT_PROXY_CRLFILE },
	{ "CURLOPT_PROXY_KEYPASSWD",                 CURLOPT_PROXY_KEYPASSWD },
	{ "CURLOPT_PROXY_PINNEDPUBLICKEY",           CURLOPT_PROXY_PINNEDPUBLICKEY },
	{ "CURLOPT_PROXY_SSL_CIPHER_LIST",           CURLOPT_PROXY_SSL_CIPHER_LIST },
	{ "CURLOPT_PROXY_SSL_OPTIONS",               CURLOPT_PROXY_SSL_OPTIONS },
	{ "CURLOPT_PROXY_SSL_VERIFYHOST",            CURLOPT_PROXY_SSL_VERIFYHOST },
	{ "CURLOPT_PROXY_SSL_VERIFYPEER",            CURLOPT_PROXY_SSL_VERIFYPEER },
	{ "CURLOPT_PROXY_SSLCERT",                   CURLOPT_PROXY_SSLCERT },
	{ "CURLOPT_PROXY_SSLCERTTYPE",               CURLOPT_PROXY_SSLCERTTYPE },
	{ "CURLOPT_PROXY_SSLKEY",                    CURLOPT_PROXY_SSLKEY },
	{ "CURLOPT_PROXY_SSLKEYTYPE",                CURLOPT_PROXY_SSLKEYTYPE },
	{ "CURLOPT_PROXY_SSLVERSION",                CURLOPT_PROXY_SSLVERSION },
	{ "CURLOPT_PROXY_TLSAUTH_PASSWORD",          CURLOPT_PROXY_TLSAUTH_PASSWORD },
	{ "CURLOPT_PROXY_TLSAUTH_TYPE",              CURLOPT_PROXY_TLSAUTH_TYPE },
	{ "CURLOPT_PROXY_TLSAUTH_USERNAME",          CURLOPT_PROXY_TLSAUTH_USERNAME },
	{ "CURLPROXY_HTTPS",                         CURLPROXY_HTTPS },
	{ "CURL_MAX_READ_SIZE",                      CURL_MAX_READ_SIZE },
	{ "CURLOPT_ABSTRACT_UNIX_SOCKET",            CURLOPT_ABSTRACT_UNIX_SOCKET },
	{ "CURL_SSLVERSION_MAX_DEFAULT",             CURL_SSLVERSION_MAX_DEFAULT },
	{ "CURL_SSLVERSION_MAX_NONE",                CURL_SSLVERSION_MAX_NONE },
	{ "CURL_SSLVERSION_MAX_TLSv1_0",             CURL_SSLVERSION_MAX_TLSv1_0 },
	{ "CURL_SSLVERSION_MAX_TLSv1_1",             CURL_SSLVERSION_MAX_TLSv1_1 },
	{ "CURL_SSLVERSION_MAX_TLSv1_2",             CURL_SSLVERSION_MAX_TLSv1_2 },
	{ "CURL_SSLVERSION_MAX_TLSv1_3",             CURL_SSLVERSION_MAX_TLSv1_3 },
	{ "CURLOPT_SUPPRESS_CONNECT_HEADERS",        CURLOPT_SUPPRESS_CONNECT_HEADERS },
	{ "CURLAUTH_GSSAPI",                         CURLAUTH_GSSAPI },
	{ "CURLINFO_CONTENT_LENGTH_DOWNLOAD_T",      CURLINFO_CONTENT_LENGTH_DOWNLOAD_T },
	{ "CURLINFO_CONTENT_LENGTH_UPLOAD_T",        CURLINFO_CONTENT_LENGTH_UPLOAD_T },
	{ "CURLINFO_SIZE_DOWNLOAD_T",                CURLINFO_SIZE_DOWNLOAD_T },
	{ "CURLINFO_SIZE_UPLOAD_T",                  CURLINFO_SIZE_UPLOAD_T },
	{ "CURLINFO_SPEED_DOWNLOAD_T",               CURLINFO_SPEED_DOWNLOAD_T },
	{ "CURLINFO_SPEED_UPLOAD_T",                 CURLINFO_SPEED_UPLOAD_T },
	{ "CURLOPT_REQUEST_TARGET",                  CURLOPT_REQUEST_TARGET },
	{ "CURLOPT_SOCKS5_AUTH",                     CURLOPT_SOCKS5_AUTH },
	{ "CURLOPT_SSH_COMPRESSION",                 CURLOPT_SSH_COMPRESSION },
	{ "CURL_VERSION_MULTI_SSL",                  CURL_VERSION_MULTI_SSL },
	{ "CURL_VERSION_BROTLI",                     CURL_VERSION_BROTLI },
	{ "CURL_LOCK_DATA_CONNECT",                  CURL_LOCK_DATA_CONNECT },
	{ "CURLSSH_AUTH_GSSAPI",                     CURLSSH_AUTH_GSSAPI },
	{ "CURLINFO_FILETIME_T",                     CURLINFO_FILETIME_T },
	{ "CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS",       CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS },
	{ "CURLOPT_TIMEVALUE_LARGE",                 CURLOPT_TIMEVALUE_LARGE },
	{ "CURLOPT_DNS_SHUFFLE_ADDRESSES",           CURLOPT_DNS_SHUFFLE_ADDRESSES },
	{ "CURLOPT_HAPROXYPROTOCOL",                 CURLOPT_HAPROXYPROTOCOL },
	{ "CURL_LOCK_DATA_PSL",                      CURL_LOCK_DATA_PSL },
	{ "CURLAUTH_BEARER",                         CURLAUTH_BEARER },
	{ "CURLINFO_APPCONNECT_TIME_T",              CURLINFO_APPCONNECT_TIME_T },
	{ "CURLINFO_CONNECT_TIME_T",                 CURLINFO_CONNECT_TIME_T },
	{ "CURLINFO_NAMELOOKUP_TIME_T",              CURLINFO_NAMELOOKUP_TIME_T },
	{ "CURLINFO_PRETRANSFER_TIME_T",             CURLINFO_PRETRANSFER_TIME_T },
	{ "CURLINFO_REDIRECT_TIME_T",                CURLINFO_REDIRECT_TIME_T },
	{ "CURLINFO_STARTTRANSFER_TIME_T",           CURLINFO_STARTTRANSFER_TIME_T },
	{ "CURLINFO_TOTAL_TIME_T",                   CURLINFO_TOTAL_TIME_T },
	{ "CURLINFO_CONN_ID",                        CURLINFO_CONN_ID },
	{ "CURLOPT_DISALLOW_USERNAME_IN_URL",        CURLOPT_DISALLOW_USERNAME_IN_URL },
	{ "CURLOPT_PROXY_TLS13_CIPHERS",             CURLOPT_PROXY_TLS13_CIPHERS },
	{ "CURLOPT_TLS13_CIPHERS",                   CURLOPT_TLS13_CIPHERS },
	{ "CURLOPT_DOH_URL",                         CURLOPT_DOH_URL },
	{ "CURLOPT_UPKEEP_INTERVAL_MS",              CURLOPT_UPKEEP_INTERVAL_MS },
	{ "CURLOPT_UPLOAD_BUFFERSIZE",               CURLOPT_UPLOAD_BUFFERSIZE },
	{ "CURLOPT_HTTP09_ALLOWED",                  CURLOPT_HTTP09_ALLOWED },
	{ "CURLALTSVC_H1",                           CURLALTSVC_H1 },
	{ "CURLALTSVC_H2",                           CURLALTSVC_H2 },
	{ "CURLALTSVC_H3",                           CURLALTSVC_H3 },
	{ "CURLALTSVC_READONLYFILE",                 CURLALTSVC_READONLYFILE },
	{ "CURLOPT_ALTSVC",                          CURLOPT_ALTSVC },
	{ "CURLOPT_ALTSVC_CTRL",                     CURLOPT_ALTSVC_CTRL },
	{ "CURL_VERSION_ALTSVC",                     CURL_VERSION_ALTSVC },
	{ "CURLOPT_MAXAGE_CONN",                     CURLOPT_MAXAGE_CONN },
	{ "CURLOPT_SASL_AUTHZID",                    CURLOPT_SASL_AUTHZID },
	{ "CURL_VERSION_HTTP3",                      CURL_VERSION_HTTP3 },
	{ "CURLINFO_RETRY_AFTER",                    CURLINFO_RETRY_AFTER },
	{ "CURL_HTTP_VERSION_3",                     CURL_HTTP_VERSION_3 },
	{ "CURLMOPT_MAX_CONCURRENT_STREAMS",         CURLMOPT_MAX_CONCURRENT_STREAMS },
	{ "CURLSSLOPT_NO_PARTIALCHAIN",              CURLSSLOPT_NO_PARTIALCHAIN },
	{ "CURLOPT_MAIL_RCPT_ALLLOWFAILS",           CURLOPT_MAIL_RCPT_ALLLOWFAILS },
	{ "CURLSSLOPT_REVOKE_BEST_EFFORT",           CURLSSLOPT_REVOKE_BEST_EFFORT },
	{ "CURLOPT_ISSUERCERT_BLOB",                 CURLOPT_ISSUERCERT_BLOB },
	{ "CURLOPT_PROXY_ISSUERCERT",                CURLOPT_PROXY_ISSUERCERT },
	{ "CURLOPT_PROXY_ISSUERCERT_BLOB",           CURLOPT_PROXY_ISSUERCERT_BLOB },
	{ "CURLOPT_PROXY_SSLCERT_BLOB",              CURLOPT_PROXY_SSLCERT_BLOB },
	{ "CURLOPT_PROXY_SSLKEY_BLOB",               CURLOPT_PROXY_SSLKEY_BLOB },
	{ "CURLOPT_SSLCERT_BLOB",                    CURLOPT_SSLCERT_BLOB },
	{ "CURLOPT_SSLKEY_BLOB",                     CURLOPT_SSLKEY_BLOB },
	{ "CURLPROTO_MQTT",                          CURLPROTO_MQTT },
	{ "CURLSSLOPT_NATIVE_CA",                    CURLSSLOPT_NATIVE_CA },
	{ "CURL_VERSION_UNICODE",                    CURL_VERSION_UNICODE },
	{ "CURL_VERSION_ZSTD",                       CURL_VERSION_ZSTD },
	{ "CURLE_PROXY",                             CURLE_PROXY },
	{ "CURLINFO_PROXY_ERROR",                    CURLINFO_PROXY_ERROR },
	{ "CURLOPT_SSL_EC_CURVES",                   CURLOPT_SSL_EC_CURVES },
	{ "CURLPX_BAD_ADDRESS_TYPE",                 CURLPX_BAD_ADDRESS_TYPE },
	{ "CURLPX_BAD_VERSION",                      CURLPX_BAD_VERSION },
	{ "CURLPX_CLOSED",                           CURLPX_CLOSED },
	{ "CURLPX_GSSAPI",                           CURLPX_GSSAPI },
	{ "CURLPX_GSSAPI_PERMSG",                    CURLPX_GSSAPI_PERMSG },
	{ "CURLPX_GSSAPI_PROTECTION",                CURLPX_GSSAPI_PROTECTION },
	{ "CURLPX_IDENTD",                           CURLPX_IDENTD },
	{ "CURLPX_IDENTD_DIFFER",                    CURLPX_IDENTD_DIFFER },
	{ "CURLPX_LONG_HOSTNAME",                    CURLPX_LONG_HOSTNAME },
	{ "CURLPX_LONG_PASSWD",                      CURLPX_LONG_PASSWD },
	{ "CURLPX_LONG_USER",                        CURLPX_LONG_USER },
	{ "CURLPX_NO_AUTH",                          CURLPX_NO_AUTH },
	{ "CURLPX_OK",                               CURLPX_OK },
	{ "CURLPX_RECV_ADDRESS",                     CURLPX_RECV_ADDRESS },
	{ "CURLPX_RECV_AUTH",                        CURLPX_RECV_AUTH },
	{ "CURLPX_RECV_CONNECT",                     CURLPX_RECV_CONNECT },
	{ "CURLPX_RECV_REQACK",                      CURLPX_RECV_REQACK },
	{ "CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED", CURLPX_REPLY_ADDRESS_TYPE_NOT_SUPPORTED },
	{ "CURLPX_REPLY_COMMAND_NOT_SUPPORTED",      CURLPX_REPLY_COMMAND_NOT_SUPPORTED },
	{ "CURLPX_REPLY_CONNECTION_REFUSED",         CURLPX_REPLY_CONNECTION_REFUSED },
	{ "CURLPX_REPLY_GENERAL_SERVER_FAILURE",     CURLPX_REPLY_GENERAL_SERVER_FAILURE },
	{ "CURLPX_REPLY_HOST_UNREACHABLE",           CURLPX_REPLY_HOST_UNREACHABLE },
	{ "CURLPX_REPLY_NETWORK_UNREACHABLE",        CURLPX_REPLY_NETWORK_UNREACHABLE },
	{ "CURLPX_REPLY_NOT_ALLOWED",                CURLPX_REPLY_NOT_ALLOWED },
	{ "CURLPX_REPLY_TTL_EXPIRED",                CURLPX_REPLY_TTL_EXPIRED },
	{ "CURLPX_REPLY_UNASSIGNED",                 CURLPX_REPLY_UNASSIGNED },
	{ "CURLPX_REQUEST_FAILED",                   CURLPX_REQUEST_FAILED },
	{ "CURLPX_RESOLVE_HOST",                     CURLPX_RESOLVE_HOST },
	{ "CURLPX_SEND_AUTH",                        CURLPX_SEND_AUTH },
	{ "CURLPX_SEND_CONNECT",                     CURLPX_SEND_CONNECT },
	{ "CURLPX_SEND_REQUEST",                     CURLPX_SEND_REQUEST },
	{ "CURLPX_UNKNOWN_FAIL",                     CURLPX_UNKNOWN_FAIL },
	{ "CURLPX_UNKNOWN_MODE",                     CURLPX_UNKNOWN_MODE },
	{ "CURLPX_USER_REJECTED",                    CURLPX_USER_REJECTED },
	{ "CURLHSTS_ENABLE",                         CURLHSTS_ENABLE },
	{ "CURLHSTS_READONLYFILE",                   CURLHSTS_READONLYFILE },
	{ "CURLOPT_HSTS",                            CURLOPT_HSTS },
	{ "CURLOPT_HSTS_CTRL",                       CURLOPT_HSTS_CTRL },
	{ "CURL_VERSION_HSTS",                       CURL_VERSION_HSTS },
	{ "CURLAUTH_AWS_SIGV4",                      CURLAUTH_AWS_SIGV4 },
	{ "CURLOPT_AWS_SIGV4",                       CURLOPT_AWS_SIGV4 },
	{ "CURLINFO_REFERER",                        CURLINFO_REFERER },
	{ "CURLOPT_DOH_SSL_VERIFYHOST",              CURLOPT_DOH_SSL_VERIFYHOST },
	{ "CURLOPT_DOH_SSL_VERIFYPEER",              CURLOPT_DOH_SSL_VERIFYPEER },
	{ "CURLOPT_DOH_SSL_VERIFYSTATUS",            CURLOPT_DOH_SSL_VERIFYSTATUS },
	{ "CURL_VERSION_GSASL",                      CURL_VERSION_GSASL },
	{ "CURLOPT_CAINFO_BLOB",                     CURLOPT_CAINFO_BLOB },
	{ "CURLOPT_PROXY_CAINFO_BLOB",               CURLOPT_PROXY_CAINFO_BLOB },
	{ "CURLSSLOPT_AUTO_CLIENT_CERT",             CURLSSLOPT_AUTO_CLIENT_CERT },
	{ "CURLOPT_MAXLIFETIME_CONN",                CURLOPT_MAXLIFETIME_CONN },
	{ "CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256",      CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256 },
	{ "CURLOPT_PREREQFUNCTION",                  CURLOPT_PREREQFUNCTION },
	{ "CURL_PREREQFUNC_OK",                      CURL_PREREQFUNC_OK },
	{ "CURL_PREREQFUNC_ABORT",                   CURL_PREREQFUNC_ABORT },
	{ "CURLOPT_MIME_OPTIONS",                    CURLOPT_MIME_OPTIONS },
	{ "CURLMIMEOPT_FORMESCAPE",                  CURLMIMEOPT_FORMESCAPE },
	{ "CURLOPT_SSH_HOSTKEYFUNCTION",             CURLOPT_SSH_HOSTKEYFUNCTION },
	{ "CURLOPT_PROTOCOLS_STR",                   CURLOPT_PROTOCOLS_STR },
	{ "CURLOPT_REDIR_PROTOCOLS_STR",             CURLOPT_REDIR_PROTOCOLS_STR },
	{ "CURLOPT_WS_OPTIONS",                      CURLOPT_WS_OPTIONS },
	{ "CURLWS_RAW_MODE",                         CURLWS_RAW_MODE },
	{ "CURLOPT_CA_CACHE_TIMEOUT",                CURLOPT_CA_CACHE_TIMEOUT },
	{ "CURLOPT_QUICK_EXIT",                      CURLOPT_QUICK_EXIT },
	{ "CURL_HTTP_VERSION_3ONLY",                 CURL_HTTP_VERSION_3ONLY },
	{ "CURLOPT_SAFE_UPLOAD",                     -1 },
};

static void CurlConstExpand(ph7_value *pVal,void *pUserData)
{
	ph7_value_int64(pVal,((const struct CurlConstant *)pUserData)->iValue);
}

PH7_PRIVATE void PH7_RegisterCurlConstants(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){
		ph7_create_constant(&(*pVm),aCurlConst[n].zName,CurlConstExpand,
			(void *)&aCurlConst[n]);
	}
}

/* ===== curl_version() ===== */

/*
 * php's feature-name table: the bit each name reports, in php's own order.
 * The names are php's spelling, not libcurl's constant tails ("GSS-Negotiate",
 * "krb4", "TLS-SRP", "NTLMWB", "CharConv"), and all 29 were verified against
 * both the oracle's feature_list and curl_version_info()'s features word.
 */
static const struct CurlFeatureName {
	const char *zName;
	unsigned int iBit;
} aCurlFeature[] = {
	{ "AsynchDNS",     CURL_VERSION_ASYNCHDNS     },
	{ "CharConv",      CURL_VERSION_CONV          },
	{ "Debug",         CURL_VERSION_DEBUG         },
	{ "GSS-Negotiate", CURL_VERSION_GSSNEGOTIATE  },
	{ "IDN",           CURL_VERSION_IDN           },
	{ "IPv6",          CURL_VERSION_IPV6          },
	{ "krb4",          CURL_VERSION_KERBEROS4     },
	{ "Largefile",     CURL_VERSION_LARGEFILE     },
	{ "libz",          CURL_VERSION_LIBZ          },
	{ "NTLM",          CURL_VERSION_NTLM          },
	{ "NTLMWB",        CURL_VERSION_NTLM_WB       },
	{ "SPNEGO",        CURL_VERSION_SPNEGO        },
	{ "SSL",           CURL_VERSION_SSL           },
	{ "SSPI",          CURL_VERSION_SSPI          },
	{ "TLS-SRP",       CURL_VERSION_TLSAUTH_SRP   },
	{ "HTTP2",         CURL_VERSION_HTTP2         },
	{ "GSSAPI",        CURL_VERSION_GSSAPI        },
	{ "KERBEROS5",     CURL_VERSION_KERBEROS5     },
	{ "UNIX_SOCKETS",  CURL_VERSION_UNIX_SOCKETS  },
	{ "PSL",           CURL_VERSION_PSL           },
	{ "HTTPS_PROXY",   CURL_VERSION_HTTPS_PROXY   },
	{ "MULTI_SSL",     CURL_VERSION_MULTI_SSL     },
	{ "BROTLI",        CURL_VERSION_BROTLI        },
	{ "ALTSVC",        CURL_VERSION_ALTSVC        },
	{ "HTTP3",         CURL_VERSION_HTTP3         },
	{ "UNICODE",       CURL_VERSION_UNICODE       },
	{ "ZSTD",          CURL_VERSION_ZSTD          },
	{ "HSTS",          CURL_VERSION_HSTS          },
	{ "GSASL",         CURL_VERSION_GSASL         }
};

/* An int entry on the answer. */
static void CurlVersionAddInt(ph7_value *pArray,ph7_value *pWorker,
	const char *zKey,sxi64 iVal)
{
	ph7_value_int64(pWorker,iVal);
	ph7_array_add_strkey_elem(pArray,zKey,pWorker);
}

/*
 * A string entry on the answer. php prints the EMPTY STRING for a field
 * libcurl left NULL -- `ares` is NULL in every build without c-ares and the
 * oracle still answers string(0) "" for it, never null.
 */
static void CurlVersionAddStr(ph7_value *pArray,ph7_value *pWorker,
	const char *zKey,const char *zVal)
{
	ph7_value_reset_string_cursor(pWorker);
	ph7_value_string(pWorker,zVal ? zVal : "",-1);
	ph7_array_add_strkey_elem(pArray,zKey,pWorker);
	ph7_value_reset_string_cursor(pWorker);
}

/* array|false curl_version() */
static int vm_builtin_curl_version(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	curl_version_info_data *pInfo;
	ph7_value *pArray,*pWorker,*pFeature,*pProto;
	const char * const *pzProto;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_CurlGlobalInit();
	pInfo = curl_version_info(CURLVERSION_NOW);
	if( pInfo == 0 ){
		/* php's own guard: the library answered nothing, so neither do we. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray   = ph7_context_new_array(pCtx);
	pWorker  = ph7_context_new_scalar(pCtx);
	pFeature = ph7_context_new_array(pCtx);
	pProto   = ph7_context_new_array(pCtx);
	if( pArray == 0 || pWorker == 0 || pFeature == 0 || pProto == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* The key ORDER is php's and is user-visible through print_r/var_dump and
	 * a foreach; it is not libcurl's struct order. */
	CurlVersionAddInt(pArray,pWorker,"version_number",(sxi64)pInfo->version_num);
	CurlVersionAddInt(pArray,pWorker,"age",(sxi64)pInfo->age);
	CurlVersionAddInt(pArray,pWorker,"features",(sxi64)pInfo->features);
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlFeature) ; ++n ){
		ph7_value_bool(pWorker,(pInfo->features & aCurlFeature[n].iBit) ? 1 : 0);
		ph7_array_add_strkey_elem(pFeature,aCurlFeature[n].zName,pWorker);
	}
	ph7_array_add_strkey_elem(pArray,"feature_list",pFeature);
	CurlVersionAddInt(pArray,pWorker,"ssl_version_number",(sxi64)pInfo->ssl_version_num);
	CurlVersionAddStr(pArray,pWorker,"version",pInfo->version);
	CurlVersionAddStr(pArray,pWorker,"host",pInfo->host);
	CurlVersionAddStr(pArray,pWorker,"ssl_version",pInfo->ssl_version);
	CurlVersionAddStr(pArray,pWorker,"libz_version",pInfo->libz_version);
	for( pzProto = pInfo->protocols ; pzProto && *pzProto ; ++pzProto ){
		ph7_value_reset_string_cursor(pWorker);
		ph7_value_string(pWorker,*pzProto,-1);
		ph7_array_add_elem(pProto,0,pWorker);
	}
	ph7_value_reset_string_cursor(pWorker);
	ph7_array_add_strkey_elem(pArray,"protocols",pProto);
	/*
	 * Everything below is gated on the struct's OWN age, which is what php
	 * gates on -- and php stops at the brotli block even against a library
	 * reporting age 10, so zstd_version and the fields after it are absent
	 * from the answer even where libcurl reports them. Reproducing php means
	 * stopping here too, not exposing what the library happens to know.
	 */
	if( pInfo->age >= CURLVERSION_SECOND ){
		CurlVersionAddStr(pArray,pWorker,"ares",pInfo->ares);
		CurlVersionAddInt(pArray,pWorker,"ares_num",(sxi64)pInfo->ares_num);
	}
	if( pInfo->age >= CURLVERSION_THIRD ){
		CurlVersionAddStr(pArray,pWorker,"libidn",pInfo->libidn);
	}
	if( pInfo->age >= CURLVERSION_FOURTH ){
		CurlVersionAddInt(pArray,pWorker,"iconv_ver_num",(sxi64)pInfo->iconv_ver_num);
		CurlVersionAddStr(pArray,pWorker,"libssh_version",pInfo->libssh_version);
	}
	if( pInfo->age >= CURLVERSION_FIFTH ){
		CurlVersionAddInt(pArray,pWorker,"brotli_ver_num",(sxi64)pInfo->brotli_ver_num);
		CurlVersionAddStr(pArray,pWorker,"brotli_version",pInfo->brotli_version);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}

/* ===== The three strerror families ===== */

/*
 * All three are pass-throughs, and the pass-through is the point: the codes
 * are C enums, so php narrows its zend_long argument to the enum's width
 * before the library ever sees it. That truncation is user-visible --
 * curl_multi_strerror(PHP_INT_MAX) answers "Please call curl_multi_perform()
 * soon" because the value arrives as -1 (CURLM_CALL_MULTI_PERFORM), not
 * because PHP_INT_MAX means anything. Casting to int here reproduces it; a
 * range check would not.
 *
 * The declared return type is ?string for the same reason php's is: the
 * answer is whatever the library's pointer says, including NULL.
 */
static int CurlStrError(ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *(*xStr)(int))
{
	int iCode = nArg > 0 ? (int)ph7_value_to_int64(apArg[0]) : 0;
	const char *zMsg = xStr(iCode);
	if( zMsg == 0 ){
		ph7_result_null(pCtx);
	}else{
		ph7_result_string(pCtx,zMsg,-1);
	}
	return PH7_OK;
}
static const char * CurlEasyStrErrorTrampoline(int iCode)
{
	return curl_easy_strerror((CURLcode)iCode);
}
static const char * CurlMultiStrErrorTrampoline(int iCode)
{
	return curl_multi_strerror((CURLMcode)iCode);
}
static const char * CurlShareStrErrorTrampoline(int iCode)
{
	return curl_share_strerror((CURLSHcode)iCode);
}
/* ?string curl_strerror(int $error_code) */
static int vm_builtin_curl_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlEasyStrErrorTrampoline);
}
/* ?string curl_multi_strerror(int $error_code) */
static int vm_builtin_curl_multi_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlMultiStrErrorTrampoline);
}
/* ?string curl_share_strerror(int $error_code) */
static int vm_builtin_curl_share_strerror(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return CurlStrError(pCtx,nArg,apArg,CurlShareStrErrorTrampoline);
}

/* ===== curl_setopt() ===== */

/*
 * What php DOES with the value it is handed, per option.
 *
 * The kinds were derived by sweeping all 269 options against 17 value types
 * and reading what php answered; they are not libcurl's own typing, though
 * they mostly follow from it. libcurl encodes a type in the option NUMBER
 * (below 10000 long, 10000-19999 pointer, 20000+ function, 30000+ off_t,
 * 40000+ blob) and php's switch agrees with that bucket for 247 of the 269 --
 * the exceptions are the whole reason this is a table rather than arithmetic:
 * ten pointer options take an ARRAY php turns into a curl_slist, five take a
 * php STREAM, and six are php's own (RETURNTRANSFER and BINARYTRANSFER exist
 * in no libcurl, PRIVATE stores a php value, SHARE takes a share handle,
 * SAFE_UPLOAD refuses to be turned off, DNS_USE_GLOBAL_CACHE is accepted and
 * ignored).
 *
 * The table is also the VALIDATOR. php does not ask libcurl whether an option
 * exists: an unknown number never reaches the library, it falls off the end of
 * php's switch. So a number a newer libcurl knows and this table does not is a
 * ValueError here exactly as it is in php built against the older library.
 */
#define CURL_OPT_LONG       0   /* zval -> long, straight to libcurl */
#define CURL_OPT_STRING     1   /* zval -> string, NUL-screened */
#define CURL_OPT_SLIST      2   /* array -> curl_slist owned by the handle */
#define CURL_OPT_CALLBACK   3   /* a php callable (its own slice) */
#define CURL_OPT_FILE       4   /* a php stream (its own slice) */
#define CURL_OPT_SAFEUP     5   /* php's own: truthy only */
#define CURL_OPT_RETURN     6   /* php's own: exec answers the body */
#define CURL_OPT_PRIVATE    7   /* php's own: stores the value itself */
#define CURL_OPT_SHARE      8   /* php's own: a CurlShareHandle */
#define CURL_OPT_POSTFIELDS 9   /* string or array (the upload slice) */
#define CURL_OPT_IGNORE    10   /* accepted and read by nothing, like php */

static const struct CurlOptDef {
	sxi64 iOpt;
	int iKind;
} aCurlOpt[] = {
	{ -1,                                CURL_OPT_SAFEUP     },  /* -1 */
	{ CURLOPT_PORT,                      CURL_OPT_LONG       },  /* 3 */
	{ CURLOPT_TIMEOUT,                   CURL_OPT_LONG       },  /* 13 */
	{ CURLOPT_INFILESIZE,                CURL_OPT_LONG       },  /* 14 */
	{ CURLOPT_LOW_SPEED_LIMIT,           CURL_OPT_LONG       },  /* 19 */
	{ CURLOPT_LOW_SPEED_TIME,            CURL_OPT_LONG       },  /* 20 */
	{ CURLOPT_RESUME_FROM,               CURL_OPT_LONG       },  /* 21 */
	{ CURLOPT_CRLF,                      CURL_OPT_LONG       },  /* 27 */
	{ CURLOPT_SSLVERSION,                CURL_OPT_LONG       },  /* 32 */
	{ CURLOPT_TIMECONDITION,             CURL_OPT_LONG       },  /* 33 */
	{ CURLOPT_TIMEVALUE,                 CURL_OPT_LONG       },  /* 34 */
	{ CURLOPT_VERBOSE,                   CURL_OPT_LONG       },  /* 41 */
	{ CURLOPT_HEADER,                    CURL_OPT_LONG       },  /* 42 */
	{ CURLOPT_NOPROGRESS,                CURL_OPT_LONG       },  /* 43 */
	{ CURLOPT_NOBODY,                    CURL_OPT_LONG       },  /* 44 */
	{ CURLOPT_FAILONERROR,               CURL_OPT_LONG       },  /* 45 */
	{ CURLOPT_UPLOAD,                    CURL_OPT_LONG       },  /* 46 */
	{ CURLOPT_POST,                      CURL_OPT_LONG       },  /* 47 */
	{ CURLOPT_DIRLISTONLY,               CURL_OPT_LONG       },  /* 48 */
	{ CURLOPT_FTPLISTONLY,               CURL_OPT_LONG       },  /* 48 */
	{ CURLOPT_APPEND,                    CURL_OPT_LONG       },  /* 50 */
	{ CURLOPT_FTPAPPEND,                 CURL_OPT_LONG       },  /* 50 */
	{ CURLOPT_NETRC,                     CURL_OPT_LONG       },  /* 51 */
	{ CURLOPT_FOLLOWLOCATION,            CURL_OPT_LONG       },  /* 52 */
	{ CURLOPT_TRANSFERTEXT,              CURL_OPT_LONG       },  /* 53 */
	{ CURLOPT_PUT,                       CURL_OPT_LONG       },  /* 54 */
	{ CURLOPT_AUTOREFERER,               CURL_OPT_LONG       },  /* 58 */
	{ CURLOPT_PROXYPORT,                 CURL_OPT_LONG       },  /* 59 */
	{ CURLOPT_HTTPPROXYTUNNEL,           CURL_OPT_LONG       },  /* 61 */
	{ CURLOPT_SSL_VERIFYPEER,            CURL_OPT_LONG       },  /* 64 */
	{ CURLOPT_MAXREDIRS,                 CURL_OPT_LONG       },  /* 68 */
	{ CURLOPT_FILETIME,                  CURL_OPT_LONG       },  /* 69 */
	{ CURLOPT_MAXCONNECTS,               CURL_OPT_LONG       },  /* 71 */
	{ CURLOPT_FRESH_CONNECT,             CURL_OPT_LONG       },  /* 74 */
	{ CURLOPT_FORBID_REUSE,              CURL_OPT_LONG       },  /* 75 */
	{ CURLOPT_CONNECTTIMEOUT,            CURL_OPT_LONG       },  /* 78 */
	{ CURLOPT_HTTPGET,                   CURL_OPT_LONG       },  /* 80 */
	{ CURLOPT_SSL_VERIFYHOST,            CURL_OPT_LONG       },  /* 81 */
	{ CURLOPT_HTTP_VERSION,              CURL_OPT_LONG       },  /* 84 */
	{ CURLOPT_FTP_USE_EPSV,              CURL_OPT_LONG       },  /* 85 */
	{ CURLOPT_SSLENGINE_DEFAULT,         CURL_OPT_LONG       },  /* 90 */
	{ CURLOPT_DNS_USE_GLOBAL_CACHE,      CURL_OPT_IGNORE     },  /* 91 */
	{ CURLOPT_DNS_CACHE_TIMEOUT,         CURL_OPT_LONG       },  /* 92 */
	{ CURLOPT_COOKIESESSION,             CURL_OPT_LONG       },  /* 96 */
	{ CURLOPT_BUFFERSIZE,                CURL_OPT_LONG       },  /* 98 */
	{ CURLOPT_NOSIGNAL,                  CURL_OPT_LONG       },  /* 99 */
	{ CURLOPT_PROXYTYPE,                 CURL_OPT_LONG       },  /* 101 */
	{ CURLOPT_UNRESTRICTED_AUTH,         CURL_OPT_LONG       },  /* 105 */
	{ CURLOPT_FTP_USE_EPRT,              CURL_OPT_LONG       },  /* 106 */
	{ CURLOPT_HTTPAUTH,                  CURL_OPT_LONG       },  /* 107 */
	{ CURLOPT_FTP_CREATE_MISSING_DIRS,   CURL_OPT_LONG       },  /* 110 */
	{ CURLOPT_PROXYAUTH,                 CURL_OPT_LONG       },  /* 111 */
	{ CURLOPT_FTP_RESPONSE_TIMEOUT,      CURL_OPT_LONG       },  /* 112 */
	{ CURLOPT_SERVER_RESPONSE_TIMEOUT,   CURL_OPT_LONG       },  /* 112 */
	{ CURLOPT_IPRESOLVE,                 CURL_OPT_LONG       },  /* 113 */
	{ CURLOPT_MAXFILESIZE,               CURL_OPT_LONG       },  /* 114 */
	{ CURLOPT_FTP_SSL,                   CURL_OPT_LONG       },  /* 119 */
	{ CURLOPT_USE_SSL,                   CURL_OPT_LONG       },  /* 119 */
	{ CURLOPT_TCP_NODELAY,               CURL_OPT_LONG       },  /* 121 */
	{ CURLOPT_FTPSSLAUTH,                CURL_OPT_LONG       },  /* 129 */
	{ CURLOPT_IGNORE_CONTENT_LENGTH,     CURL_OPT_LONG       },  /* 136 */
	{ CURLOPT_FTP_SKIP_PASV_IP,          CURL_OPT_LONG       },  /* 137 */
	{ CURLOPT_FTP_FILEMETHOD,            CURL_OPT_LONG       },  /* 138 */
	{ CURLOPT_LOCALPORT,                 CURL_OPT_LONG       },  /* 139 */
	{ CURLOPT_LOCALPORTRANGE,            CURL_OPT_LONG       },  /* 140 */
	{ CURLOPT_CONNECT_ONLY,              CURL_OPT_LONG       },  /* 141 */
	{ CURLOPT_SSL_SESSIONID_CACHE,       CURL_OPT_LONG       },  /* 150 */
	{ CURLOPT_SSH_AUTH_TYPES,            CURL_OPT_LONG       },  /* 151 */
	{ CURLOPT_FTP_SSL_CCC,               CURL_OPT_LONG       },  /* 154 */
	{ CURLOPT_TIMEOUT_MS,                CURL_OPT_LONG       },  /* 155 */
	{ CURLOPT_CONNECTTIMEOUT_MS,         CURL_OPT_LONG       },  /* 156 */
	{ CURLOPT_HTTP_TRANSFER_DECODING,    CURL_OPT_LONG       },  /* 157 */
	{ CURLOPT_HTTP_CONTENT_DECODING,     CURL_OPT_LONG       },  /* 158 */
	{ CURLOPT_NEW_FILE_PERMS,            CURL_OPT_LONG       },  /* 159 */
	{ CURLOPT_NEW_DIRECTORY_PERMS,       CURL_OPT_LONG       },  /* 160 */
	{ CURLOPT_POSTREDIR,                 CURL_OPT_LONG       },  /* 161 */
	{ CURLOPT_PROXY_TRANSFER_MODE,       CURL_OPT_LONG       },  /* 166 */
	{ CURLOPT_ADDRESS_SCOPE,             CURL_OPT_LONG       },  /* 171 */
	{ CURLOPT_CERTINFO,                  CURL_OPT_LONG       },  /* 172 */
	{ CURLOPT_TFTP_BLKSIZE,              CURL_OPT_LONG       },  /* 178 */
	{ CURLOPT_SOCKS5_GSSAPI_NEC,         CURL_OPT_LONG       },  /* 180 */
	{ CURLOPT_PROTOCOLS,                 CURL_OPT_LONG       },  /* 181 */
	{ CURLOPT_REDIR_PROTOCOLS,           CURL_OPT_LONG       },  /* 182 */
	{ CURLOPT_FTP_USE_PRET,              CURL_OPT_LONG       },  /* 188 */
	{ CURLOPT_RTSP_REQUEST,              CURL_OPT_LONG       },  /* 189 */
	{ CURLOPT_RTSP_CLIENT_CSEQ,          CURL_OPT_LONG       },  /* 193 */
	{ CURLOPT_RTSP_SERVER_CSEQ,          CURL_OPT_LONG       },  /* 194 */
	{ CURLOPT_WILDCARDMATCH,             CURL_OPT_LONG       },  /* 197 */
	{ CURLOPT_TRANSFER_ENCODING,         CURL_OPT_LONG       },  /* 207 */
	{ CURLOPT_GSSAPI_DELEGATION,         CURL_OPT_LONG       },  /* 210 */
	{ CURLOPT_ACCEPTTIMEOUT_MS,          CURL_OPT_LONG       },  /* 212 */
	{ CURLOPT_TCP_KEEPALIVE,             CURL_OPT_LONG       },  /* 213 */
	{ CURLOPT_TCP_KEEPIDLE,              CURL_OPT_LONG       },  /* 214 */
	{ CURLOPT_TCP_KEEPINTVL,             CURL_OPT_LONG       },  /* 215 */
	{ CURLOPT_SSL_OPTIONS,               CURL_OPT_LONG       },  /* 216 */
	{ CURLOPT_SASL_IR,                   CURL_OPT_LONG       },  /* 218 */
	{ CURLOPT_SSL_ENABLE_NPN,            CURL_OPT_LONG       },  /* 225 */
	{ CURLOPT_SSL_ENABLE_ALPN,           CURL_OPT_LONG       },  /* 226 */
	{ CURLOPT_EXPECT_100_TIMEOUT_MS,     CURL_OPT_LONG       },  /* 227 */
	{ CURLOPT_HEADEROPT,                 CURL_OPT_LONG       },  /* 229 */
	{ CURLOPT_SSL_VERIFYSTATUS,          CURL_OPT_LONG       },  /* 232 */
	{ CURLOPT_SSL_FALSESTART,            CURL_OPT_LONG       },  /* 233 */
	{ CURLOPT_PATH_AS_IS,                CURL_OPT_LONG       },  /* 234 */
	{ CURLOPT_PIPEWAIT,                  CURL_OPT_LONG       },  /* 237 */
	{ CURLOPT_STREAM_WEIGHT,             CURL_OPT_LONG       },  /* 239 */
	{ CURLOPT_TFTP_NO_OPTIONS,           CURL_OPT_LONG       },  /* 242 */
	{ CURLOPT_TCP_FASTOPEN,              CURL_OPT_LONG       },  /* 244 */
	{ CURLOPT_KEEP_SENDING_ON_ERROR,     CURL_OPT_LONG       },  /* 245 */
	{ CURLOPT_PROXY_SSL_VERIFYPEER,      CURL_OPT_LONG       },  /* 248 */
	{ CURLOPT_PROXY_SSL_VERIFYHOST,      CURL_OPT_LONG       },  /* 249 */
	{ CURLOPT_PROXY_SSLVERSION,          CURL_OPT_LONG       },  /* 250 */
	{ CURLOPT_PROXY_SSL_OPTIONS,         CURL_OPT_LONG       },  /* 261 */
	{ CURLOPT_SUPPRESS_CONNECT_HEADERS,  CURL_OPT_LONG       },  /* 265 */
	{ CURLOPT_SOCKS5_AUTH,               CURL_OPT_LONG       },  /* 267 */
	{ CURLOPT_SSH_COMPRESSION,           CURL_OPT_LONG       },  /* 268 */
	{ CURLOPT_HAPPY_EYEBALLS_TIMEOUT_MS, CURL_OPT_LONG       },  /* 271 */
	{ CURLOPT_HAPROXYPROTOCOL,           CURL_OPT_LONG       },  /* 274 */
	{ CURLOPT_DNS_SHUFFLE_ADDRESSES,     CURL_OPT_LONG       },  /* 275 */
	{ CURLOPT_DISALLOW_USERNAME_IN_URL,  CURL_OPT_LONG       },  /* 278 */
	{ CURLOPT_UPLOAD_BUFFERSIZE,         CURL_OPT_LONG       },  /* 280 */
	{ CURLOPT_UPKEEP_INTERVAL_MS,        CURL_OPT_LONG       },  /* 281 */
	{ CURLOPT_HTTP09_ALLOWED,            CURL_OPT_LONG       },  /* 285 */
	{ CURLOPT_ALTSVC_CTRL,               CURL_OPT_LONG       },  /* 286 */
	{ CURLOPT_MAXAGE_CONN,               CURL_OPT_LONG       },  /* 288 */
	{ CURLOPT_MAIL_RCPT_ALLLOWFAILS,     CURL_OPT_LONG       },  /* 290 */
	{ CURLOPT_HSTS_CTRL,                 CURL_OPT_LONG       },  /* 299 */
	{ CURLOPT_DOH_SSL_VERIFYPEER,        CURL_OPT_LONG       },  /* 306 */
	{ CURLOPT_DOH_SSL_VERIFYHOST,        CURL_OPT_LONG       },  /* 307 */
	{ CURLOPT_DOH_SSL_VERIFYSTATUS,      CURL_OPT_LONG       },  /* 308 */
	{ CURLOPT_MAXLIFETIME_CONN,          CURL_OPT_LONG       },  /* 314 */
	{ CURLOPT_MIME_OPTIONS,              CURL_OPT_LONG       },  /* 315 */
	{ CURLOPT_WS_OPTIONS,                CURL_OPT_LONG       },  /* 320 */
	{ CURLOPT_CA_CACHE_TIMEOUT,          CURL_OPT_LONG       },  /* 321 */
	{ CURLOPT_QUICK_EXIT,                CURL_OPT_LONG       },  /* 322 */
	{ CURLOPT_FILE,                      CURL_OPT_FILE       },  /* 10001 */
	{ CURLOPT_URL,                       CURL_OPT_STRING     },  /* 10002 */
	{ CURLOPT_PROXY,                     CURL_OPT_STRING     },  /* 10004 */
	{ CURLOPT_USERPWD,                   CURL_OPT_STRING     },  /* 10005 */
	{ CURLOPT_PROXYUSERPWD,              CURL_OPT_STRING     },  /* 10006 */
	{ CURLOPT_RANGE,                     CURL_OPT_STRING     },  /* 10007 */
	{ CURLOPT_INFILE,                    CURL_OPT_FILE       },  /* 10009 */
	{ CURLOPT_READDATA,                  CURL_OPT_FILE       },  /* 10009 */
	{ CURLOPT_POSTFIELDS,                CURL_OPT_POSTFIELDS },  /* 10015 */
	{ CURLOPT_REFERER,                   CURL_OPT_STRING     },  /* 10016 */
	{ CURLOPT_FTPPORT,                   CURL_OPT_STRING     },  /* 10017 */
	{ CURLOPT_USERAGENT,                 CURL_OPT_STRING     },  /* 10018 */
	{ CURLOPT_COOKIE,                    CURL_OPT_STRING     },  /* 10022 */
	{ CURLOPT_HTTPHEADER,                CURL_OPT_SLIST      },  /* 10023 */
	{ CURLOPT_SSLCERT,                   CURL_OPT_STRING     },  /* 10025 */
	{ CURLOPT_KEYPASSWD,                 CURL_OPT_STRING     },  /* 10026 */
	{ CURLOPT_SSLCERTPASSWD,             CURL_OPT_STRING     },  /* 10026 */
	{ CURLOPT_SSLKEYPASSWD,              CURL_OPT_STRING     },  /* 10026 */
	{ CURLOPT_QUOTE,                     CURL_OPT_SLIST      },  /* 10028 */
	{ CURLOPT_WRITEHEADER,               CURL_OPT_FILE       },  /* 10029 */
	{ CURLOPT_COOKIEFILE,                CURL_OPT_STRING     },  /* 10031 */
	{ CURLOPT_CUSTOMREQUEST,             CURL_OPT_STRING     },  /* 10036 */
	{ CURLOPT_STDERR,                    CURL_OPT_FILE       },  /* 10037 */
	{ CURLOPT_POSTQUOTE,                 CURL_OPT_SLIST      },  /* 10039 */
	{ CURLOPT_INTERFACE,                 CURL_OPT_STRING     },  /* 10062 */
	{ CURLOPT_KRB4LEVEL,                 CURL_OPT_STRING     },  /* 10063 */
	{ CURLOPT_KRBLEVEL,                  CURL_OPT_STRING     },  /* 10063 */
	{ CURLOPT_CAINFO,                    CURL_OPT_STRING     },  /* 10065 */
	{ CURLOPT_TELNETOPTIONS,             CURL_OPT_SLIST      },  /* 10070 */
	{ CURLOPT_RANDOM_FILE,               CURL_OPT_STRING     },  /* 10076 */
	{ CURLOPT_EGDSOCKET,                 CURL_OPT_STRING     },  /* 10077 */
	{ CURLOPT_COOKIEJAR,                 CURL_OPT_STRING     },  /* 10082 */
	{ CURLOPT_SSL_CIPHER_LIST,           CURL_OPT_STRING     },  /* 10083 */
	{ CURLOPT_SSLCERTTYPE,               CURL_OPT_STRING     },  /* 10086 */
	{ CURLOPT_SSLKEY,                    CURL_OPT_STRING     },  /* 10087 */
	{ CURLOPT_SSLKEYTYPE,                CURL_OPT_STRING     },  /* 10088 */
	{ CURLOPT_SSLENGINE,                 CURL_OPT_STRING     },  /* 10089 */
	{ CURLOPT_PREQUOTE,                  CURL_OPT_SLIST      },  /* 10093 */
	{ CURLOPT_CAPATH,                    CURL_OPT_STRING     },  /* 10097 */
	{ CURLOPT_SHARE,                     CURL_OPT_SHARE      },  /* 10100 */
	{ CURLOPT_ACCEPT_ENCODING,           CURL_OPT_STRING     },  /* 10102 */
	{ CURLOPT_ENCODING,                  CURL_OPT_STRING     },  /* 10102 */
	{ CURLOPT_PRIVATE,                   CURL_OPT_PRIVATE    },  /* 10103 */
	{ CURLOPT_HTTP200ALIASES,            CURL_OPT_SLIST      },  /* 10104 */
	{ CURLOPT_NETRC_FILE,                CURL_OPT_STRING     },  /* 10118 */
	{ CURLOPT_FTP_ACCOUNT,               CURL_OPT_STRING     },  /* 10134 */
	{ CURLOPT_COOKIELIST,                CURL_OPT_STRING     },  /* 10135 */
	{ CURLOPT_FTP_ALTERNATIVE_TO_USER,   CURL_OPT_STRING     },  /* 10147 */
	{ CURLOPT_SSH_PUBLIC_KEYFILE,        CURL_OPT_STRING     },  /* 10152 */
	{ CURLOPT_SSH_PRIVATE_KEYFILE,       CURL_OPT_STRING     },  /* 10153 */
	{ CURLOPT_SSH_HOST_PUBLIC_KEY_MD5,   CURL_OPT_STRING     },  /* 10162 */
	{ CURLOPT_CRLFILE,                   CURL_OPT_STRING     },  /* 10169 */
	{ CURLOPT_ISSUERCERT,                CURL_OPT_STRING     },  /* 10170 */
	{ CURLOPT_USERNAME,                  CURL_OPT_STRING     },  /* 10173 */
	{ CURLOPT_PASSWORD,                  CURL_OPT_STRING     },  /* 10174 */
	{ CURLOPT_PROXYUSERNAME,             CURL_OPT_STRING     },  /* 10175 */
	{ CURLOPT_PROXYPASSWORD,             CURL_OPT_STRING     },  /* 10176 */
	{ CURLOPT_NOPROXY,                   CURL_OPT_STRING     },  /* 10177 */
	{ CURLOPT_SOCKS5_GSSAPI_SERVICE,     CURL_OPT_STRING     },  /* 10179 */
	{ CURLOPT_SSH_KNOWNHOSTS,            CURL_OPT_STRING     },  /* 10183 */
	{ CURLOPT_MAIL_FROM,                 CURL_OPT_STRING     },  /* 10186 */
	{ CURLOPT_MAIL_RCPT,                 CURL_OPT_SLIST      },  /* 10187 */
	{ CURLOPT_RTSP_SESSION_ID,           CURL_OPT_STRING     },  /* 10190 */
	{ CURLOPT_RTSP_STREAM_URI,           CURL_OPT_STRING     },  /* 10191 */
	{ CURLOPT_RTSP_TRANSPORT,            CURL_OPT_STRING     },  /* 10192 */
	{ CURLOPT_RESOLVE,                   CURL_OPT_SLIST      },  /* 10203 */
	{ CURLOPT_TLSAUTH_USERNAME,          CURL_OPT_STRING     },  /* 10204 */
	{ CURLOPT_TLSAUTH_PASSWORD,          CURL_OPT_STRING     },  /* 10205 */
	{ CURLOPT_TLSAUTH_TYPE,              CURL_OPT_STRING     },  /* 10206 */
	{ CURLOPT_DNS_SERVERS,               CURL_OPT_STRING     },  /* 10211 */
	{ CURLOPT_MAIL_AUTH,                 CURL_OPT_STRING     },  /* 10217 */
	{ CURLOPT_XOAUTH2_BEARER,            CURL_OPT_STRING     },  /* 10220 */
	{ CURLOPT_DNS_INTERFACE,             CURL_OPT_STRING     },  /* 10221 */
	{ CURLOPT_DNS_LOCAL_IP4,             CURL_OPT_STRING     },  /* 10222 */
	{ CURLOPT_DNS_LOCAL_IP6,             CURL_OPT_STRING     },  /* 10223 */
	{ CURLOPT_LOGIN_OPTIONS,             CURL_OPT_STRING     },  /* 10224 */
	{ CURLOPT_PROXYHEADER,               CURL_OPT_SLIST      },  /* 10228 */
	{ CURLOPT_PINNEDPUBLICKEY,           CURL_OPT_STRING     },  /* 10230 */
	{ CURLOPT_UNIX_SOCKET_PATH,          CURL_OPT_STRING     },  /* 10231 */
	{ CURLOPT_PROXY_SERVICE_NAME,        CURL_OPT_STRING     },  /* 10235 */
	{ CURLOPT_SERVICE_NAME,              CURL_OPT_STRING     },  /* 10236 */
	{ CURLOPT_DEFAULT_PROTOCOL,          CURL_OPT_STRING     },  /* 10238 */
	{ CURLOPT_CONNECT_TO,                CURL_OPT_SLIST      },  /* 10243 */
	{ CURLOPT_PROXY_CAINFO,              CURL_OPT_STRING     },  /* 10246 */
	{ CURLOPT_PROXY_CAPATH,              CURL_OPT_STRING     },  /* 10247 */
	{ CURLOPT_PROXY_TLSAUTH_USERNAME,    CURL_OPT_STRING     },  /* 10251 */
	{ CURLOPT_PROXY_TLSAUTH_PASSWORD,    CURL_OPT_STRING     },  /* 10252 */
	{ CURLOPT_PROXY_TLSAUTH_TYPE,        CURL_OPT_STRING     },  /* 10253 */
	{ CURLOPT_PROXY_SSLCERT,             CURL_OPT_STRING     },  /* 10254 */
	{ CURLOPT_PROXY_SSLCERTTYPE,         CURL_OPT_STRING     },  /* 10255 */
	{ CURLOPT_PROXY_SSLKEY,              CURL_OPT_STRING     },  /* 10256 */
	{ CURLOPT_PROXY_SSLKEYTYPE,          CURL_OPT_STRING     },  /* 10257 */
	{ CURLOPT_PROXY_KEYPASSWD,           CURL_OPT_STRING     },  /* 10258 */
	{ CURLOPT_PROXY_SSL_CIPHER_LIST,     CURL_OPT_STRING     },  /* 10259 */
	{ CURLOPT_PROXY_CRLFILE,             CURL_OPT_STRING     },  /* 10260 */
	{ CURLOPT_PRE_PROXY,                 CURL_OPT_STRING     },  /* 10262 */
	{ CURLOPT_PROXY_PINNEDPUBLICKEY,     CURL_OPT_STRING     },  /* 10263 */
	{ CURLOPT_ABSTRACT_UNIX_SOCKET,      CURL_OPT_STRING     },  /* 10264 */
	{ CURLOPT_REQUEST_TARGET,            CURL_OPT_STRING     },  /* 10266 */
	{ CURLOPT_TLS13_CIPHERS,             CURL_OPT_STRING     },  /* 10276 */
	{ CURLOPT_PROXY_TLS13_CIPHERS,       CURL_OPT_STRING     },  /* 10277 */
	{ CURLOPT_DOH_URL,                   CURL_OPT_STRING     },  /* 10279 */
	{ CURLOPT_ALTSVC,                    CURL_OPT_STRING     },  /* 10287 */
	{ CURLOPT_SASL_AUTHZID,              CURL_OPT_STRING     },  /* 10289 */
	{ CURLOPT_PROXY_ISSUERCERT,          CURL_OPT_STRING     },  /* 10296 */
	{ CURLOPT_SSL_EC_CURVES,             CURL_OPT_STRING     },  /* 10298 */
	{ CURLOPT_HSTS,                      CURL_OPT_STRING     },  /* 10300 */
	{ CURLOPT_AWS_SIGV4,                 CURL_OPT_STRING     },  /* 10305 */
	{ CURLOPT_SSH_HOST_PUBLIC_KEY_SHA256, CURL_OPT_STRING     },  /* 10311 */
	{ CURLOPT_PROTOCOLS_STR,             CURL_OPT_STRING     },  /* 10318 */
	{ CURLOPT_REDIR_PROTOCOLS_STR,       CURL_OPT_STRING     },  /* 10319 */
	{ 19913,                             CURL_OPT_RETURN     },  /* 19913 */
	{ 19914,                             CURL_OPT_IGNORE     },  /* 19914 */
	{ CURLOPT_WRITEFUNCTION,             CURL_OPT_CALLBACK   },  /* 20011 */
	{ CURLOPT_READFUNCTION,              CURL_OPT_CALLBACK   },  /* 20012 */
	{ CURLOPT_PROGRESSFUNCTION,          CURL_OPT_CALLBACK   },  /* 20056 */
	{ CURLOPT_HEADERFUNCTION,            CURL_OPT_CALLBACK   },  /* 20079 */
	{ CURLOPT_DEBUGFUNCTION,             CURL_OPT_CALLBACK   },  /* 20094 */
	{ CURLOPT_FNMATCH_FUNCTION,          CURL_OPT_CALLBACK   },  /* 20200 */
	{ CURLOPT_XFERINFOFUNCTION,          CURL_OPT_CALLBACK   },  /* 20219 */
	{ CURLOPT_PREREQFUNCTION,            CURL_OPT_CALLBACK   },  /* 20312 */
	{ CURLOPT_SSH_HOSTKEYFUNCTION,       CURL_OPT_CALLBACK   },  /* 20316 */
	{ CURLOPT_INFILESIZE_LARGE,          CURL_OPT_LONG       },  /* 30115 */
	{ CURLOPT_MAXFILESIZE_LARGE,         CURL_OPT_LONG       },  /* 30117 */
	{ CURLOPT_MAX_SEND_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30145 */
	{ CURLOPT_MAX_RECV_SPEED_LARGE,      CURL_OPT_LONG       },  /* 30146 */
	{ CURLOPT_TIMEVALUE_LARGE,           CURL_OPT_LONG       },  /* 30270 */
	{ CURLOPT_SSLCERT_BLOB,              CURL_OPT_STRING     },  /* 40291 */
	{ CURLOPT_SSLKEY_BLOB,               CURL_OPT_STRING     },  /* 40292 */
	{ CURLOPT_PROXY_SSLCERT_BLOB,        CURL_OPT_STRING     },  /* 40293 */
	{ CURLOPT_PROXY_SSLKEY_BLOB,         CURL_OPT_STRING     },  /* 40294 */
	{ CURLOPT_ISSUERCERT_BLOB,           CURL_OPT_STRING     },  /* 40295 */
	{ CURLOPT_PROXY_ISSUERCERT_BLOB,     CURL_OPT_STRING     },  /* 40297 */
	{ CURLOPT_CAINFO_BLOB,               CURL_OPT_STRING     },  /* 40309 */
	{ CURLOPT_PROXY_CAINFO_BLOB,         CURL_OPT_STRING     },  /* 40310 */
};

/*
 * The option's php NAME, for the diagnostics that print one ("The
 * CURLOPT_HTTPHEADER option must have an array value"). Read out of the
 * constant table rather than repeated, so the two can never disagree.
 */
static const char * CurlOptName(sxi64 iOpt)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlConst) ; ++n ){
		if( aCurlConst[n].iValue == iOpt
		 && SyStrncmp(aCurlConst[n].zName,"CURLOPT_",sizeof("CURLOPT_")-1) == 0 ){
			return aCurlConst[n].zName;
		}
	}
	return "CURLOPT_UNKNOWN";
}

static const struct CurlOptDef * CurlOptFind(sxi64 iOpt)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aCurlOpt) ; ++n ){
		if( aCurlOpt[n].iOpt == iOpt ){
			return &aCurlOpt[n];
		}
	}
	return 0;
}

/* ===== The handle verbs ===== */

/*
 * A bare instance of one of the handle classes. `new` on them is refused
 * (php's "Cannot directly construct ..."), and this is the door the refusal
 * leaves open: the engine's own creation step, never the opcode's.
 */
static ph7_class_instance * CurlNewInstance(ph7_vm *pVm,const char *zName,int nName)
{
	ph7_class *pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,0,0);
	return pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
}

/* CurlHandle|false curl_init(?string $url = null) */
static int vm_builtin_curl_init(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis;
	phl_curl *pCurl;
	const char *zUrl = 0;
	int nUrl = 0;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		zUrl = ph7_value_to_string(apArg[0],&nUrl);
		/*
		 * php screens the URL for an embedded NUL before libcurl sees it,
		 * because libcurl takes a C string and would silently stop at the
		 * byte. The sentence says "cURL option" even though the argument is
		 * $url -- it is the shared option-setter's wording, reached from here.
		 */
		if( SyByteFind(zUrl,(sxu32)nUrl,0,0) == SXRET_OK ){
			/* The status a throw installs IS the builtin's status: answering
			 * PH7_OK leaves the throw half-raised, and the next script run in
			 * the same interpreter prints nothing at all. */
			ph7_result_bool(pCtx,0);
			return PH7_VmThrowException(pCtx,"ValueError",
				"curl_init(): cURL option must not contain any null bytes");
		}
	}
	pCurl = CurlNewHandle(pVm);
	if( pCurl == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pThis = CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);
	if( pThis == 0 || CurlAttach(pThis,pCurl) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( zUrl ){
		curl_easy_setopt(pCurl->pEasy,CURLOPT_URL,zUrl);
	}
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/*
 * void curl_close(CurlHandle $handle)
 *
 * A NO-OP, which is the whole finding. php 8 turned the resource into an
 * object, and the object's own teardown is what frees the handle -- so after
 * curl_close() the handle still works: curl_setopt() answers true, curl_exec()
 * runs the transfer, curl_errno() reports it. Freeing here (which is what the
 * name says and what php 7 did) would make every one of those a use-after-free
 * on a script php runs happily.
 */
static int vm_builtin_curl_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * void curl_reset(CurlHandle $handle)
 *
 * Every OPTION goes back to its default -- and the error state does NOT.
 * curl_reset() on a handle whose last transfer failed leaves curl_errno()
 * reporting that failure; only the option setters clear it. (Probing the verbs
 * one at a time is what shows this: a sweep that resets after a setopt sees a
 * cleared errno and credits the wrong verb.)
 */
static int vm_builtin_curl_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	if( pCurl && pCurl->pEasy ){
		curl_easy_reset(pCurl->pEasy);
		/* curl_easy_reset() drops the error buffer with everything else. */
		curl_easy_setopt(pCurl->pEasy,CURLOPT_ERRORBUFFER,pCurl->zErrBuf);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * php's error state after a REFUSED option: errno 48 and libcurl's own text
 * for it. The ValueError is thrown and the handle still reports the failure --
 * `curl_setopt($h, 999999, 1)` in a try/catch leaves curl_errno() at 48 -- so
 * the two are not alternatives, they both happen.
 */
static void CurlSetErr(phl_curl *pCurl,int iCode)
{
	const char *zMsg;
	if( pCurl == 0 ){
		return;
	}
	pCurl->iLastErr = iCode;
	zMsg = curl_easy_strerror((CURLcode)iCode);
	pCurl->zErrBuf[0] = 0;
	if( zMsg ){
		sxu32 nMsg = SyStrlen(zMsg);
		if( nMsg > sizeof(pCurl->zErrBuf) - 1 ){ nMsg = sizeof(pCurl->zErrBuf) - 1; }
		SyMemcpy(zMsg,pCurl->zErrBuf,nMsg);
		pCurl->zErrBuf[nMsg] = 0;
	}
}
/*
 * Every setter clears the handle's error FIRST (curl_setopt and
 * curl_setopt_array are two of the three verbs that do; curl_upkeep is the
 * third, and curl_reset notably is not).
 */
static void CurlClearErr(phl_curl *pCurl)
{
	if( pCurl ){
		pCurl->iLastErr = 0;
		pCurl->zErrBuf[0] = 0;
	}
}
/*
 * A string option's value. php stringifies ANYTHING for these -- null becomes
 * "", an array becomes "Array" with the ordinary conversion warning, an object
 * is the ordinary "could not be converted to string" Error -- and then screens
 * the result for a NUL, because libcurl takes a C string and would silently
 * stop at the byte. The screen is on the whole VALUE, which is why the same
 * sentence appears from curl_init().
 *
 * A slist ELEMENT is stringified the same way but is NOT screened: php lets a
 * header carrying a NUL through, which is measured, not assumed.
 */
static int CurlSetString(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,sxi32 *pRc)
{
	const char *zVal;
	int nVal = 0;
	sxi32 rcSv;
	/* The USER-VISIBLE coercion, not the embedder API: an array has to warn
	 * "Array to string conversion" and an object with no __toString() has to
	 * be php's catchable Error, both of which the silent ph7_value_to_string()
	 * skips. */
	rcSv = PH7_ValueToStringUV(pCtx,pVal,&zVal,&nVal);
	if( rcSv != SXRET_OK ){
		/* The coercion threw. Its status is the BUILTIN's status: swallowing it
		 * and answering PH7_OK leaves the engine with a half-installed throw,
		 * and the next script run in the same interpreter prints nothing. */
		*pRc = rcSv;
		return -1;
	}
	if( SyByteFind(zVal,(sxu32)nVal,0,0) == SXRET_OK ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"curl_setopt(): cURL option must not contain any null bytes");
		return -1;
	}
	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,zVal) == CURLE_OK ? 1 : 0;
}
/*
 * A long option's value, with the one option php screens by hand.
 * CURLOPT_SSL_VERIFYHOST no longer has a meaningful 1: libcurl treats it as 2,
 * and php says so at E_NOTICE before passing 2 along -- so a program that
 * still writes 1 gets php's sentence, not silence.
 */
static int CurlSetLong(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal)
{
	sxi64 iVal = ph7_value_to_int64(pVal);
	if( iOpt == CURLOPT_SSL_VERIFYHOST && iVal == 1 ){
		/* ph7_context_throw_error already prints "curl_setopt(): " */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,
			"CURLOPT_SSL_VERIFYHOST no longer accepts the value 1, "
			"value 2 will be used instead");
		iVal = 2;
	}
	return curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,(long)iVal) == CURLE_OK ? 1 : 0;
}
/*
 * An slist option's value: the array's VALUES in order, keys ignored, each
 * stringified. libcurl does not copy the list, so the handle owns it until the
 * option is set again or the handle is freed -- which is what pSlist is for.
 */
struct CurlSlistBuild {
	ph7_context *pCtx;
	struct curl_slist *pList;
	int bFailed;
	int bThrew;
	sxi32 rcThrow;
};
static int CurlSlistWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)
{
	struct CurlSlistBuild *pB = (struct CurlSlistBuild *)pUser;
	struct curl_slist *pNext;
	const char *zVal;
	int nVal = 0;
	SXUNUSED(pKey);
	if( pB->bFailed ){
		return PH7_OK;
	}
	/* Same user-visible coercion the scalar options get -- an object in a
	 * header list is php's Error, not a silent "Object" -- but NO null-byte
	 * screen: php lets a list element carrying one through. */
	{
		sxi32 rcSv = PH7_ValueToStringUV(pB->pCtx,pVal,&zVal,&nVal);
		if( rcSv != SXRET_OK ){
			pB->bFailed = 1;
			pB->bThrew = 1;
			pB->rcThrow = rcSv;
			return PH7_ABORT;
		}
	}
	/* The value is a TEMPORARY the walker owns for this call only, and
	 * curl_slist_append copies it, so nothing is kept past the return. */
	pNext = curl_slist_append(pB->pList,zVal ? zVal : "");
	if( pNext == 0 ){
		pB->bFailed = 1;
		return PH7_OK;
	}
	pB->pList = pNext;
	return PH7_OK;
}
static int CurlSetSlist(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,
	const char *zOptName,sxi32 *pRc)
{
	struct CurlSlistBuild sB;
	phl_curl_slist *pSlot;
	if( !ph7_value_is_array(pVal) ){
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"curl_setopt(): The %s option must have an array value",zOptName);
		return -1;
	}
	sB.pCtx = pCtx;
	sB.pList = 0;
	sB.bFailed = 0;
	sB.bThrew = 0;
	sB.rcThrow = PH7_OK;
	ph7_array_walk(pVal,CurlSlistWalk,&sB);
	if( sB.bFailed ){
		if( sB.pList ){
			curl_slist_free_all(sB.pList);
		}
		if( sB.bThrew ){
			*pRc = sB.rcThrow;
			return -1;
		}
		return 0;
	}
	/* Replace whatever this option held before: the previous list stays alive
	 * until libcurl has been pointed at the new one. */
	pSlot = CurlSlistSlot(pCurl,iOpt);
	if( pSlot == 0 ){
		if( sB.pList ){
			curl_slist_free_all(sB.pList);
		}
		return 0;
	}
	if( curl_easy_setopt(pCurl->pEasy,(CURLoption)iOpt,sB.pList) != CURLE_OK ){
		if( sB.pList ){
			curl_slist_free_all(sB.pList);
		}
		return 0;
	}
	if( pSlot->pList ){
		curl_slist_free_all(pSlot->pList);
	}
	pSlot->pList = sB.pList;
	return 1;
}
/*
 * One option, the whole switch. Answers 1 (true), 0 (false) or -1 (a throw is
 * already installed).
 */
static int CurlSetOne(ph7_context *pCtx,phl_curl *pCurl,sxi64 iOpt,ph7_value *pVal,
	const char *zFunc,sxi32 *pRc)
{
	const struct CurlOptDef *pDef = CurlOptFind(iOpt);
	if( pDef == 0 ){
		/* php's switch has no arm for it, so the library never sees it -- and
		 * the handle records CURLE_UNKNOWN_OPTION all the same. */
		CurlSetErr(pCurl,CURLE_UNKNOWN_OPTION);
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #2 ($option) is not a valid cURL option",zFunc);
		return -1;
	}
	switch( pDef->iKind ){
	case CURL_OPT_LONG:
		return CurlSetLong(pCtx,pCurl,iOpt,pVal);
	case CURL_OPT_STRING:
		return CurlSetString(pCtx,pCurl,iOpt,pVal,pRc);
	case CURL_OPT_SLIST:
		return CurlSetSlist(pCtx,pCurl,iOpt,pVal,CurlOptName(iOpt),pRc);
	case CURL_OPT_SAFEUP:
		/* php's -1: safe uploads cannot be turned off any more, and the
		 * refusal is on the VALUE's truthiness, not its type. */
		if( !ph7_value_to_bool(pVal) ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Disabling safe uploads is no longer supported",zFunc);
			return -1;
		}
		return 1;
	case CURL_OPT_IGNORE:
		return 1;
	default:
		break;
	}
	/* The kinds whose own slices have not landed: a callable, a stream, the
	 * share handle, PRIVATE's stored value and POSTFIELDS. Refusing loudly
	 * beats answering true and transferring something else. */
	*pRc = PH7_VmThrowException(pCtx,"Error",
		"%s(): option %s is not implemented yet in this build",zFunc,CurlOptName(iOpt));
	return -1;
}
/* bool curl_setopt(CurlHandle $handle, int $option, mixed $value) */
static int vm_builtin_curl_setopt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	sxi32 rcOut = PH7_OK;
	int rc;
	if( pCurl == 0 || pCurl->pEasy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	CurlClearErr(pCurl);
	rc = CurlSetOne(pCtx,pCurl,ph7_value_to_int64(apArg[1]),apArg[2],"curl_setopt",&rcOut);
	ph7_result_bool(pCtx,rc == 1);
	return rcOut;
}
/*
 * bool curl_setopt_array(CurlHandle $handle, array $options)
 *
 * The options are applied IN ORDER and the walk stops at the first refusal --
 * a bad third entry leaves the first two applied. Its two ValueErrors are not
 * curl_setopt's and are not each other's: a key that is not an option NUMBER
 * says "must contain only valid cURL options", a key that is not an integer at
 * all says "contains an invalid cURL option". (A numeric STRING key is neither:
 * php's array normalizes it to an int before this ever sees it.)
 */
struct CurlSetoptArray {
	ph7_context *pCtx;
	phl_curl *pCurl;
	int rc;          /* 1 all applied, 0 a false, -1 a throw is installed */
	sxi32 rcOut;     /* the status a throwing coercion wants propagated */
};
static int CurlSetoptArrayWalk(ph7_value *pKey,ph7_value *pVal,void *pUser)
{
	struct CurlSetoptArray *pW = (struct CurlSetoptArray *)pUser;
	int rc;
	if( pW->rc != 1 ){
		return PH7_ABORT;
	}
	if( !ph7_value_is_int(pKey) ){
		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",
			"curl_setopt_array(): Argument #2 ($options) contains an invalid cURL option");
		pW->rc = -1;
		return PH7_ABORT;
	}
	if( CurlOptFind(ph7_value_to_int64(pKey)) == 0 ){
		CurlSetErr(pW->pCurl,CURLE_UNKNOWN_OPTION);
		pW->rcOut = PH7_VmThrowException(pW->pCtx,"ValueError",
			"curl_setopt_array(): Argument #2 ($options) must contain only valid cURL options");
		pW->rc = -1;
		return PH7_ABORT;
	}
	rc = CurlSetOne(pW->pCtx,pW->pCurl,ph7_value_to_int64(pKey),pVal,"curl_setopt_array",&pW->rcOut);
	if( rc != 1 ){
		pW->rc = rc;
		return PH7_ABORT;
	}
	return PH7_OK;
}
static int vm_builtin_curl_setopt_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	struct CurlSetoptArray sW;
	if( pCurl == 0 || pCurl->pEasy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	CurlClearErr(pCurl);
	sW.pCtx = pCtx;
	sW.pCurl = pCurl;
	sW.rc = 1;
	sW.rcOut = PH7_OK;
	ph7_array_walk(apArg[1],CurlSetoptArrayWalk,&sW);
	ph7_result_bool(pCtx,sW.rc == 1);
	return sW.rcOut;
}

/*
 * CurlHandle|false curl_copy_handle(CurlHandle $handle)
 *
 * The same duphandle `clone` does, through a function -- php's two spellings
 * of one operation, and they answer alike down to the clean error state on the
 * copy.
 */
static int vm_builtin_curl_copy_handle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	ph7_class_instance *pThis;
	if( pCurl == 0 || pCurl->pEasy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pThis = CurlNewInstance(pVm,"CurlHandle",sizeof("CurlHandle")-1);
	if( pThis == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	CurlInstanceClone(pVm,pThis,(ph7_class_instance *)apArg[0]->x.pOther);
	if( CurlOfInstance(pThis) == 0 ){
		/* the dup failed; hand back php's false rather than an empty handle */
		PH7_ClassInstanceUnref(pThis);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/* int curl_errno(CurlHandle $handle) */
static int vm_builtin_curl_errno(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	ph7_result_int(pCtx,pCurl ? pCurl->iLastErr : 0);
	return PH7_OK;
}
/*
 * string curl_error(CurlHandle $handle)
 *
 * The ERROR BUFFER, not curl_easy_strerror(): libcurl writes a sentence naming
 * the host and port it could not reach, where the code's own text is the
 * generic "Couldn't connect to server". Empty when nothing has failed.
 */
static int vm_builtin_curl_error(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_curl *pCurl = CurlArg(pCtx,nArg,apArg);
	if( pCurl == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,pCurl->zErrBuf,-1);
	return PH7_OK;
}

/* ===== Installation ===== */

PH7_PRIVATE sxi32 PH7_VmInstallCurl(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "curl_version",        vm_builtin_curl_version        },
		{ "curl_strerror",       vm_builtin_curl_strerror       },
		{ "curl_multi_strerror", vm_builtin_curl_multi_strerror },
		{ "curl_share_strerror", vm_builtin_curl_share_strerror },
		{ "curl_init",           vm_builtin_curl_init           },
		{ "curl_close",          vm_builtin_curl_close          },
		{ "curl_reset",          vm_builtin_curl_reset          },
		{ "curl_errno",          vm_builtin_curl_errno          },
		{ "curl_error",          vm_builtin_curl_error          },
		{ "curl_copy_handle",    vm_builtin_curl_copy_handle    },
		{ "curl_setopt",         vm_builtin_curl_setopt         },
		{ "curl_setopt_array",   vm_builtin_curl_setopt_array   }
	};
	/*
	 * The libcurl handle, and nothing else: php's CurlHandle declares no
	 * method, no constant and no property, and prints as an empty object on
	 * every presentation surface. The one slot here is engine storage, hidden
	 * so it appears on none of them.
	 */
	static const PH7_NativePropDef aProp[] = {
		{ "__res", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	/*
	 * FINAL (php refuses `class X extends CurlHandle`), NOINSTANTIATE with
	 * php's own per-class sentence, and NOSERIALIZE -- but NOT NOCLONE, which
	 * is where CurlHandle parts company with every other handle class here:
	 * php maps clone to curl_easy_duphandle(), so `clone $h` answers a second,
	 * independent handle. ReflectionClass::isInstantiable() still reports true
	 * for it, which is php's answer too, because the refusal lives in the
	 * creation step rather than in a private constructor.
	 */
	static const PH7_NativeClassSpec sSpec = {
		"CurlHandle", 0, 0,
		PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOSERIALIZE,
		0, 0, 0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		CurlInstanceRelease, 0, 0
	};
	sxu32 n;
	sxi32 rc;
	PH7_CurlGlobalInit();
	pVm->pCurlHandles = 0;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; ++n ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
	if( rc == SXRET_OK ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"CurlHandle",sizeof("CurlHandle")-1,FALSE,0);
		if( pClass ){
			pClass->zNewRefusal =
				"Cannot directly construct CurlHandle, use curl_init() instead";
			/* php's clone_obj: `clone $h` is curl_easy_duphandle(), which is
			 * why this class alone is not PH7_CLASS_NOCLONE. Stated on the
			 * MOUNTED class, like every other handler hook. */
			pClass->xClone = CurlInstanceClone;
		}
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when curl is disabled (MSVC C4206) */
typedef int vm_curl_unused;
#endif /* PH7_ENABLE_CURL */
