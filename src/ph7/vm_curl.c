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
static void CurlFreeHandle(phl_curl *pCurl)
{
	if( pCurl->pEasy ){
		curl_easy_cleanup(pCurl->pEasy);
		pCurl->pEasy = 0;
	}
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
			PH7_VmThrowException(pCtx,"ValueError",
				"curl_init(): cURL option must not contain any null bytes");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
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
		{ "curl_copy_handle",    vm_builtin_curl_copy_handle    }
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
